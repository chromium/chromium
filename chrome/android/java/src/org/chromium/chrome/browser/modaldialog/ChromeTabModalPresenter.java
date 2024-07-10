// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;

import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.modaldialog.TabModalPresenter;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.webxr.XrDelegate;
import org.chromium.components.webxr.XrDelegateProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This presenter creates tab modality by blocking interaction with select UI elements while a
 * dialog is visible.
 */
public class ChromeTabModalPresenter extends TabModalPresenter
        implements BrowserControlsStateProvider.Observer {
    /** The activity displaying the dialogs. */
    private final Activity mActivity;

    private final Supplier<TabObscuringHandler> mTabObscuringHandlerSupplier;
    private final Supplier<ToolbarManager> mToolbarManagerSupplier;
    private final Runnable mHideContextualSearch;
    private final FullscreenManager mFullscreenManager;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final TabModalBrowserControlsVisibilityDelegate mVisibilityDelegate;
    private final TabModelSelector mTabModelSelector;

    /** The active tab of which the dialog will be shown on top. */
    private Tab mActiveTab;

    /** The parent view that contains the dialog container. */
    private ViewGroup mContainerParent;

    /** Whether the dialog container is brought to the front in its parent. */
    private boolean mContainerIsAtFront;

    /**
     * Whether an enter animation on the dialog container should run when
     * {@link #onBrowserControlsFullyVisible} is called.
     */
    private boolean mRunEnterAnimationOnCallback;

    /**
     * The sibling view of the dialog container drawn next in its parent when it should be behind
     * browser controls. If BottomSheet is opened or UrlBar is focused, the dialog container should
     * be behind the browser controls and the URL suggestions.
     */
    private View mDefaultNextSiblingView;

    private int mBottomControlsHeight;
    private boolean mShouldUpdateContainerLayoutParams;

    /** A token held while the dialog manager is obscuring all tabs. */
    private TabObscuringHandler.Token mTabObscuringToken;

    /**
     * Constructor for initializing dialog container.
     *
     * @param activity The activity displaying the dialogs.
     * @param tabObscuringHandlerSupplier Supplies the {@link TabObscuringHandler} object.
     * @param toolbarManagerSupplier Supplies the {@link ToolbarManager} object.
     * @param hideContextualSearch Runnable hiding contextual search panel.
     * @param fullscreenManager The {@link FullscreenManager} object, used to exit full screen.
     * @param browserControlsVisibilityManager The {@link BrowserControlsVisibilityManager} object.
     * @param tabModelSelector The {@link TabModelSelector} object.
     */
    public ChromeTabModalPresenter(
            Activity activity,
            Supplier<TabObscuringHandler> tabObscuringHandlerSupplier,
            Supplier<ToolbarManager> toolbarManagerSupplier,
            Runnable hideContextualSearch,
            FullscreenManager fullscreenManager,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            TabModelSelector tabModelSelector) {
        super(activity);
        mActivity = activity;
        mTabObscuringHandlerSupplier = tabObscuringHandlerSupplier;
        mToolbarManagerSupplier = toolbarManagerSupplier;
        mFullscreenManager = fullscreenManager;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mBrowserControlsVisibilityManager.addObserver(this);
        mVisibilityDelegate = new TabModalBrowserControlsVisibilityDelegate();
        mHideContextualSearch = hideContextualSearch;
        mTabModelSelector = tabModelSelector;
    }

    public void destroy() {
        mBrowserControlsVisibilityManager.removeObserver(this);
    }

    /**
     * @return The browser controls visibility delegate associated with tab modal dialogs.
     */
    public BrowserControlsVisibilityDelegate getBrowserControlsVisibilityDelegate() {
        return mVisibilityDelegate;
    }

    @Override
    protected ViewGroup createDialogContainer() {
        ViewStub dialogContainerStub = mActivity.findViewById(R.id.tab_modal_dialog_container_stub);
        dialogContainerStub.setLayoutResource(R.layout.modal_dialog_container);

        ViewGroup dialogContainer = (ViewGroup) dialogContainerStub.inflate();
        dialogContainer.setVisibility(View.GONE);

        // Make sure clicks are not consumed by content beneath the container view.
        dialogContainer.setClickable(true);

        mContainerParent = (ViewGroup) dialogContainer.getParent();
        // The default sibling view is the next view of the dialog container stub in main.xml and
        // should not be removed from its parent.
        mDefaultNextSiblingView =
                mActivity.findViewById(R.id.tab_modal_dialog_container_sibling_view);
        assert mDefaultNextSiblingView != null;

        Resources resources = mActivity.getResources();

        MarginLayoutParams params = (MarginLayoutParams) dialogContainer.getLayoutParams();
        params.width = ViewGroup.MarginLayoutParams.MATCH_PARENT;
        params.height = ViewGroup.MarginLayoutParams.MATCH_PARENT;
        params.topMargin = getContainerTopMargin(resources, mBrowserControlsVisibilityManager);
        params.bottomMargin = getContainerBottomMargin(mBrowserControlsVisibilityManager);
        dialogContainer.setLayoutParams(params);

        int scrimVerticalMargin =
                resources.getDimensionPixelSize(R.dimen.tab_modal_scrim_vertical_margin);
        View scrimView = dialogContainer.findViewById(R.id.scrim);
        params = (MarginLayoutParams) scrimView.getLayoutParams();
        params.width = MarginLayoutParams.MATCH_PARENT;
        params.height = MarginLayoutParams.MATCH_PARENT;
        params.topMargin = scrimVerticalMargin;
        scrimView.setLayoutParams(params);

        return dialogContainer;
    }

    @Override
    protected void showDialogContainer() {
        if (mShouldUpdateContainerLayoutParams) {
            MarginLayoutParams params = (MarginLayoutParams) getDialogContainer().getLayoutParams();
            params.topMargin =
                    getContainerTopMargin(
                            mActivity.getResources(), mBrowserControlsVisibilityManager);
            params.bottomMargin = mBottomControlsHeight;
            getDialogContainer().setLayoutParams(params);
            mShouldUpdateContainerLayoutParams = false;
        }

        // Don't show the dialog container before browser controls are guaranteed fully visible.
        if (BrowserControlsUtils.areBrowserControlsFullyVisible(
                mBrowserControlsVisibilityManager)) {
            runEnterAnimation();
        } else {
            mRunEnterAnimationOnCallback = true;
        }
        assert mTabObscuringToken == null;
        mTabObscuringToken =
                mTabObscuringHandlerSupplier.get().obscure(TabObscuringHandler.Target.TAB_CONTENT);
    }

    @Override
    protected void setBrowserControlsAccess(boolean restricted) {
        if (!mToolbarManagerSupplier.hasValue()) return;

        View menuButton = mToolbarManagerSupplier.get().getMenuButtonView();

        if (restricted) {
            mActiveTab = mTabModelSelector.getCurrentTab();
            assert mActiveTab != null
                    : "Tab modal dialogs should be shown on top of an active tab.";

            // Hide contextual search panel so that bottom toolbar will not be
            // obscured and back press is not overridden.
            mHideContextualSearch.run();

            // Dismiss the action bar that obscures the dialogs but preserve the text selection.
            WebContents webContents = mActiveTab.getWebContents();
            if (webContents != null) {
                saveOrRestoreTextSelection(webContents, true);
            }

            // Force toolbar to show and disable overflow menu.
            onTabModalDialogStateChanged(true);

            mToolbarManagerSupplier.get().setUrlBarFocus(false, OmniboxFocusReason.UNFOCUS);

            menuButton.setEnabled(false);
        } else {
            // Show the action bar back if it was dismissed when the dialogs were showing.
            WebContents webContents = mActiveTab.getWebContents();
            if (webContents != null) {
                saveOrRestoreTextSelection(webContents, false);
            }

            onTabModalDialogStateChanged(false);
            menuButton.setEnabled(true);
            mActiveTab = null;
        }
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        mRunEnterAnimationOnCallback = false;
        mTabObscuringHandlerSupplier.get().unobscure(mTabObscuringToken);
        mTabObscuringToken = null;
        super.removeDialogView(model);
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        if (getDialogModel() == null
                || !mRunEnterAnimationOnCallback
                || !BrowserControlsUtils.areBrowserControlsFullyVisible(
                        mBrowserControlsVisibilityManager)) {
            return;
        }
        mRunEnterAnimationOnCallback = false;
        runEnterAnimation();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        mBottomControlsHeight = bottomControlsHeight;
        mShouldUpdateContainerLayoutParams = true;
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        mShouldUpdateContainerLayoutParams = true;
    }

    @Override
    public void updateContainerHierarchy(boolean toFront) {
        super.updateContainerHierarchy(toFront);

        if (toFront == mContainerIsAtFront) return;
        mContainerIsAtFront = toFront;
        if (toFront) {
            getDialogContainer().bringToFront();
        } else {
            UiUtils.removeViewFromParent(getDialogContainer());
            UiUtils.insertBefore(mContainerParent, getDialogContainer(), mDefaultNextSiblingView);
        }
    }

    /**
     * Calculate the top margin of the dialog container and the dialog scrim so that the scrim
     * doesn't overlap the toolbar.
     * @param resources {@link Resources} to use to get the scrim vertical margin.
     * @param provider {@link BrowserControlsStateProvider} for browser controls heights.
     * @return The container top margin.
     */
    public static int getContainerTopMargin(
            Resources resources, BrowserControlsStateProvider provider) {
        int scrimVerticalMargin =
                resources.getDimensionPixelSize(R.dimen.tab_modal_scrim_vertical_margin);
        return provider.getTopControlsHeight() - scrimVerticalMargin;
    }

    /**
     * Calculate the bottom margin of the dialog container.
     * @param provider {@link BrowserControlsStateProvider} for browser controls heights.
     * @return The container bottom margin.
     */
    public static int getContainerBottomMargin(BrowserControlsStateProvider provider) {
        return provider.getBottomControlsHeight();
    }

    public static boolean isDialogShowing(Tab tab) {
        return TabAttributes.from(tab).get(TabAttributeKeys.MODAL_DIALOG_SHOWING, false);
    }

    private void onTabModalDialogStateChanged(boolean isShowing) {
        TabAttributes.from(mActiveTab).set(TabAttributeKeys.MODAL_DIALOG_SHOWING, isShowing);
        mVisibilityDelegate.updateConstraintsForTab(mActiveTab);

        // AR Sessions are fullscreen sessions where it's okay to show the TabModal dialog
        // without exiting fullscreen. So if we are in one we need to ensure that we:
        // 1) Don't exit fullscreen
        // 2) Toggle the Controls visibility appropriately.
        // Note that if we don't have an XrDelegate, then we can't have an AR Session.
        XrDelegate xrDelegate = XrDelegateProvider.getDelegate();
        boolean isInArSession = (xrDelegate != null && xrDelegate.hasActiveArSession());

        // If needed, exit fullscreen mode before showing the tab modal dialog view.
        if (!isInArSession) {
            mFullscreenManager.onExitFullscreen(mActiveTab);
        }

        // Also need to update browser control state to refresh the constraints.
        if (isShowing && (areRendererInputEventsIgnored() || isInArSession)) {
            mBrowserControlsVisibilityManager.showAndroidControls(true);
        } else if (!isShowing && isInArSession) {
            mBrowserControlsVisibilityManager.restoreControlsPositions();
        } else {
            TabBrowserControlsConstraintsHelper.update(
                    mActiveTab,
                    BrowserControlsState.SHOWN,
                    !mBrowserControlsVisibilityManager.offsetOverridden());
        }
    }

    private boolean areRendererInputEventsIgnored() {
        return mActiveTab.getWebContents().getMainFrame().areInputEventsIgnored();
    }

    ViewGroup getContainerParentForTest() {
        return mContainerParent;
    }

    /** Handles browser controls constraints for the TabModal dialogs. */
    static class TabModalBrowserControlsVisibilityDelegate
            extends BrowserControlsVisibilityDelegate {
        public TabModalBrowserControlsVisibilityDelegate() {
            super(BrowserControlsState.BOTH);
        }

        /** Updates the tab modal browser constraints for the given tab. */
        public void updateConstraintsForTab(Tab tab) {
            if (tab == null) return;
            set(isDialogShowing(tab) ? BrowserControlsState.SHOWN : BrowserControlsState.BOTH);
        }
    }
}
