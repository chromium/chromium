// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorFactory;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.BooleanSupplier;

/**
 * {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a
 * {@link ChromeTabbedActivity}.
 */
public class TabbedModeTabDelegateFactory implements TabDelegateFactory {
    private final Activity mActivity;
    private final BrowserControlsVisibilityDelegate mAppBrowserControlsVisibilityDelegate;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final Runnable mContextMenuCopyLinkObserver;
    private final BottomSheetController mBottomSheetController;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final boolean mIsCustomTab;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final FullscreenManager mFullscreenManager;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final BrowserControlsManager mBrowserControlsManager;
    private final Supplier<Tab> mCurrentTabSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<Long> mLastUserInteractionTimeSupplier;
    private final BooleanSupplier mHadWarmStartSupplier;
    private final Supplier<Toolbar> mToolbarSupplier;

    private NativePageFactory mNativePageFactory;

    public TabbedModeTabDelegateFactory(Activity activity,
            BrowserControlsVisibilityDelegate appBrowserControlsVisibilityDelegate,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            Runnable contextMenuCopyLinkObserver, BottomSheetController sheetController,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate, boolean isCustomTab,
            BrowserControlsStateProvider browserControlsStateProvider,
            FullscreenManager fullscreenManager, TabCreatorManager tabCreatorManager,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            BrowserControlsManager browserControlsManager, Supplier<Tab> currentTabSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher, WindowAndroid windowAndroid,
            Supplier<Long> lastUserInteractionTimeSupplier, BooleanSupplier hadWarmStartSupplier,
            Supplier<Toolbar> toolbarSupplier) {
        mActivity = activity;
        mAppBrowserControlsVisibilityDelegate = appBrowserControlsVisibilityDelegate;
        mShareDelegateSupplier = shareDelegateSupplier;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mContextMenuCopyLinkObserver = contextMenuCopyLinkObserver;
        mBottomSheetController = sheetController;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mIsCustomTab = isCustomTab;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mFullscreenManager = fullscreenManager;
        mTabCreatorManager = tabCreatorManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mBrowserControlsManager = browserControlsManager;
        mCurrentTabSupplier = currentTabSupplier;
        mLifecycleDispatcher = lifecycleDispatcher;
        mWindowAndroid = windowAndroid;
        mLastUserInteractionTimeSupplier = lastUserInteractionTimeSupplier;
        mHadWarmStartSupplier = hadWarmStartSupplier;
        mToolbarSupplier = toolbarSupplier;
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new ActivityTabWebContentsDelegateAndroid(tab, mActivity,
                mChromeActivityNativeDelegate, /* isCustomTab= */ false,
                mBrowserControlsStateProvider, mFullscreenManager, mTabCreatorManager,
                mTabModelSelectorSupplier, mCompositorViewHolderSupplier,
                mModalDialogManagerSupplier);
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return new ExternalNavigationHandler(new ExternalNavigationDelegateImpl(tab));
    }

    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        return new ChromeContextMenuPopulatorFactory(
                new TabContextMenuItemDelegate(tab, mTabModelSelectorSupplier.get(),
                        mEphemeralTabCoordinatorSupplier, mContextMenuCopyLinkObserver,
                        mSnackbarManagerSupplier),
                mShareDelegateSupplier, ChromeContextMenuPopulator.ContextMenuMode.NORMAL,
                ExternalAuthUtils.getInstance());
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new ComposedBrowserControlsVisibilityDelegate(
                new TabStateBrowserControlsVisibilityDelegate(tab),
                mAppBrowserControlsVisibilityDelegate);
    }

    @Override
    public NativePage createNativePage(String url, NativePage candidatePage, Tab tab) {
        if (mNativePageFactory == null) {
            mNativePageFactory = new NativePageFactory(mActivity, mBottomSheetController,
                    mBrowserControlsManager, mCurrentTabSupplier, mSnackbarManagerSupplier,
                    mLifecycleDispatcher, mTabModelSelectorSupplier.get(), mShareDelegateSupplier,
                    mWindowAndroid, mLastUserInteractionTimeSupplier, mHadWarmStartSupplier,
                    mToolbarSupplier);
        }
        return mNativePageFactory.createNativePage(url, candidatePage, tab);
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
        if (mNativePageFactory != null) mNativePageFactory.destroy();
    }
}
