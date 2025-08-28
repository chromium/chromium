// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * Class responsible for handling dismissal of a tab modal dialog on user actions outside the tab
 * modal dialog.
 */
@NullMarked
public class TabModalLifetimeHandler
        implements NativeInitObserver,
                DestroyObserver,
                ModalDialogManagerObserver,
                BackPressHandler {
    /** The observer to dismiss all dialogs when the attached tab is not interactable. */
    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
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

                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    if (mActiveTab == tab) {
                        mManager.dismissDialogsOfType(
                                ModalDialogType.TAB, DialogDismissalCause.NAVIGATE);
                    }
                }
            };

    private Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ModalDialogManager mManager;
    private final Supplier<ComposedBrowserControlsVisibilityDelegate>
            mAppVisibilityDelegateSupplier;
    private final Supplier<TabObscuringHandler> mTabObscuringHandlerSupplier;
    private final Supplier<ToolbarManager> mToolbarManagerSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<BrowserControlsVisibilityManager>
            mBrowserControlsVisibilityManagerSupplier;
    private final Supplier<FullscreenManager> mFullscreenManagerSupplier;
    private final ObservableSupplierImpl<Boolean> mHandleBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<ScrimManager> mScrimManagerSupplier;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final BackPressManager mBackPressManager;
    private @MonotonicNonNull ChromeTabModalPresenter mPresenter;
    private @MonotonicNonNull TabModelSelectorTabModelObserver mTabModelObserver;
    private final Runnable mHideContextualSearch;
    private @Nullable Tab mActiveTab;
    private int mTabModalSuspendedToken;

    /**
     * @param activity The {@link Activity} that this handler is attached to.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the activity.
     * @param manager The {@link ModalDialogManager} that this handler handles.
     * @param appVisibilityDelegateSupplier Supplies the delegate that handles the application
     *     browser controls visibility.
     * @param tabObscuringHandlerSupplier Supplies the {@link TabObscuringHandler} object.
     * @param toolbarManagerSupplier Supplies the {@link ToolbarManager} object.
     * @param hideContextualSearch Runnable hiding the contextual search panel.
     * @param tabModelSelectorSupplier Supplies the {@link TabModelSelector} object.
     * @param browserControlsVisibilityManagerSupplier Supplies the {@link
     *     BrowserControlsVisibilityManager}.
     * @param fullscreenManagerSupplier Supplies the {@link FullscreenManager} object.
     * @param backPressManager The {@link BackPressManager} which can register {@link
     *     BackPressHandler}.
     * @param scrimManagerSupplier The supplier for {@link ScrimManager}. Used to darken the screen
     *     behind the dialog.
     * @param edgeToEdgeControllerSupplier The supplier for {@link EdgeToEdgeController}. Used to
     *     decide how to position the scrim.
     */
    public TabModalLifetimeHandler(
            Activity activity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ModalDialogManager manager,
            Supplier<ComposedBrowserControlsVisibilityDelegate> appVisibilityDelegateSupplier,
            Supplier<TabObscuringHandler> tabObscuringHandlerSupplier,
            Supplier<ToolbarManager> toolbarManagerSupplier,
            Runnable hideContextualSearch,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<BrowserControlsVisibilityManager> browserControlsVisibilityManagerSupplier,
            Supplier<FullscreenManager> fullscreenManagerSupplier,
            BackPressManager backPressManager,
            ObservableSupplier<ScrimManager> scrimManagerSupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        mActivity = activity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mManager = manager;
        mAppVisibilityDelegateSupplier = appVisibilityDelegateSupplier;
        mTabObscuringHandlerSupplier = tabObscuringHandlerSupplier;
        mTabModalSuspendedToken = TokenHolder.INVALID_TOKEN;
        mToolbarManagerSupplier = toolbarManagerSupplier;
        mFullscreenManagerSupplier = fullscreenManagerSupplier;
        mBrowserControlsVisibilityManagerSupplier = browserControlsVisibilityManagerSupplier;
        mHideContextualSearch = hideContextualSearch;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mBackPressManager = backPressManager;
        mManager.addObserver(this);
        backPressManager.addHandler(this, Type.TAB_MODAL_HANDLER);
        mScrimManagerSupplier = scrimManagerSupplier;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
    }

    /**
     * Notified when the focus of the omnibox has changed.
     *
     * @param hasFocus Whether the omnibox currently has focus.
     */
    public void onOmniboxFocusChanged(boolean hasFocus) {
        if (mPresenter == null) return;

        if (mPresenter.getDialogModel() != null) mPresenter.updateContainerHierarchy(!hasFocus);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        int result = shouldInterceptBackPress() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
        if (result == BackPressResult.SUCCESS) {
            assumeNonNull(mPresenter); // shouldInterceptBackPress checks if mPresenter is null.
            mPresenter.dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK);
        }
        return result;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    @Override
    public void onDialogAdded(PropertyModel model) {
        mHandleBackPressChangedSupplier.set(shouldInterceptBackPress());
    }

    @Override
    public void onDialogDismissed(PropertyModel model) {
        mHandleBackPressChangedSupplier.set(shouldInterceptBackPress());
    }

    @Override
    public void onFinishNativeInitialization() {
        assert mTabModelSelectorSupplier.get() != null;
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        assert mBrowserControlsVisibilityManagerSupplier.get() != null;
        assert mFullscreenManagerSupplier.get() != null;
        mPresenter =
                new ChromeTabModalPresenter(
                        mActivity,
                        mTabObscuringHandlerSupplier,
                        mToolbarManagerSupplier,
                        mHideContextualSearch,
                        mFullscreenManagerSupplier.get(),
                        mBrowserControlsVisibilityManagerSupplier.get(),
                        tabModelSelector,
                        mScrimManagerSupplier,
                        mEdgeToEdgeControllerSupplier);
        assert mAppVisibilityDelegateSupplier.get() != null;
        mAppVisibilityDelegateSupplier
                .get()
                .addDelegate(mPresenter.getBrowserControlsVisibilityDelegate());
        mManager.registerPresenter(mPresenter, ModalDialogType.TAB);

        handleTabChanged(tabModelSelector.getCurrentTab());
        mTabModelObserver =
                new TabModelSelectorTabModelObserver(tabModelSelector) {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        handleTabChanged(tab);
                    }
                };
    }

    private boolean shouldInterceptBackPress() {
        return mPresenter != null
                && mPresenter.getDialogModel() != null
                && mTabModalSuspendedToken == TokenHolder.INVALID_TOKEN;
    }

    private void handleTabChanged(@Nullable Tab tab) {
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
    @SuppressWarnings("NullAway")
    public void onDestroy() {
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        if (mPresenter != null) mPresenter.destroy();

        if (mActiveTab != null) {
            mActiveTab.removeObserver(mTabObserver);
            mActiveTab = null;
        }

        if (mBackPressManager.has(Type.TAB_MODAL_HANDLER)) {
            mManager.removeObserver(this);
            mBackPressManager.removeHandler(Type.TAB_MODAL_HANDLER);
        }

        mActivityLifecycleDispatcher.unregister(this);
        mActivity = null;
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
        mHandleBackPressChangedSupplier.set(shouldInterceptBackPress());
    }
}
