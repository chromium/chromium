// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.util.TokenHolder;

/**
 * Class responsible for handling dismissal of a tab modal dialog on user actions outside the tab
 * modal dialog.
 */
public class TabModalLifetimeHandler implements NativeInitObserver, Destroyable {
    /** The observer to dismiss all dialogs when the attached tab is not interactable. */
    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onInteractabilityChanged(boolean isInteractable) {
            updateSuspensionState();
        }

        @Override
        public void onDestroyed(Tab tab) {
            if (mActiveTab == tab) {
                mManager.dismissDialogsOfType(
                        ModalDialogType.TAB, DialogDismissalCause.TAB_DESTROYED);
                mActiveTab = null;
            }
        }
    };

    private final ChromeActivity mActivity;
    private final ModalDialogManager mManager;
    private TabModalPresenter mPresenter;
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private Tab mActiveTab;
    private int mTabModalSuspendedToken;

    /**
     * @param activity The {@link ChromeActivity} that this handler is attached to.
     * @param manager The {@link ModalDialogManager} that this handler handles.
     */
    public TabModalLifetimeHandler(ChromeActivity activity, ModalDialogManager manager) {
        mActivity = activity;
        mManager = manager;
        activity.getLifecycleDispatcher().register(this);
        mTabModalSuspendedToken = TokenHolder.INVALID_TOKEN;
    }

    /**
     * Notified when the focus of the omnibox has changed.
     * @param hasFocus Whether the omnibox currently has focus.
     */
    public void onOmniboxFocusChanged(boolean hasFocus) {
        if (mPresenter == null) return;

        if (mPresenter.getDialogModel() != null) mPresenter.updateContainerHierarchy(!hasFocus);
    }

    /**
     * Handle a back press event.
     */
    public boolean handleBackPress() {
        if (mPresenter == null || mPresenter.getDialogModel() == null) return false;
        mPresenter.dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        return true;
    }

    @Override
    public void onFinishNativeInitialization() {
        mPresenter = new TabModalPresenter(mActivity);
        mManager.registerPresenter(mPresenter, ModalDialogType.TAB);

        handleTabChanged(mActivity.getActivityTab());
        TabModelSelector tabModelSelector = mActivity.getTabModelSelector();
        mTabModelObserver = new TabModelSelectorTabModelObserver(tabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                handleTabChanged(tab);
            }
        };
    }

    private void handleTabChanged(Tab tab) {
        // Do not use lastId here since it can be the selected tab's ID if model is switched
        // inside tab switcher.
        if (tab != mActiveTab) {
            mManager.dismissDialogsOfType(ModalDialogType.TAB, DialogDismissalCause.TAB_SWITCHED);
            if (mActiveTab != null) mActiveTab.removeObserver(mTabObserver);

            mActiveTab = tab;
            if (mActiveTab != null) {
                mActiveTab.addObserver(mTabObserver);
                updateSuspensionState();
            }
        }
    }

    @Override
    public void destroy() {
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        if (mPresenter != null) mPresenter.destroy();
    }

    /** Update whether the {@link ModalDialogManager} should suspend tab modal dialogs. */
    private void updateSuspensionState() {
        assert mActiveTab != null;
        if (mActiveTab.isUserInteractable()) {
            mManager.resumeType(ModalDialogType.TAB, mTabModalSuspendedToken);
            mTabModalSuspendedToken = TokenHolder.INVALID_TOKEN;
        } else if (mTabModalSuspendedToken == TokenHolder.INVALID_TOKEN) {
            mTabModalSuspendedToken = mManager.suspendType(ModalDialogType.TAB);
        }
    }
}
