// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorFactory;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.metrics.StartupMetricsTracker;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.ntp_customization.edge_to_edge.TopInsetCoordinator;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.ExclusiveAccessManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.function.Supplier;

/**
 * {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a {@link
 * ChromeTabbedActivity}.
 */
@NullMarked
public class TabbedModeTabDelegateFactory implements TabDelegateFactory {
    private final Activity mActivity;
    private final BrowserControlsVisibilityDelegate mAppBrowserControlsVisibilityDelegate;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final Runnable mContextMenuCopyLinkObserver;
    private final BottomSheetController mBottomSheetController;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final FullscreenManager mFullscreenManager;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final BrowserControlsManager mBrowserControlsManager;
    private final Supplier<@Nullable Tab> mCurrentTabSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<Toolbar> mToolbarSupplier;
    private final @Nullable HomeSurfaceTracker mHomeSurfaceTracker;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;
    private final OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final ObservableSupplier<TopInsetCoordinator> mTopInsetCoordinatorSupplier;
    private final StartupMetricsTracker mStartupMetricsTracker;
    private final @Nullable ExclusiveAccessManager mExclusiveAccessManager;
    private @Nullable NativePageFactory mNativePageFactory;
    private final BackPressManager mBackPressManager;
    private final MultiInstanceManager mMultiInstanceManager;

    public TabbedModeTabDelegateFactory(
            Activity activity,
            BrowserControlsVisibilityDelegate appBrowserControlsVisibilityDelegate,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            Runnable contextMenuCopyLinkObserver,
            BottomSheetController sheetController,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            BrowserControlsStateProvider browserControlsStateProvider,
            FullscreenManager fullscreenManager,
            TabCreatorManager tabCreatorManager,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            BrowserControlsManager browserControlsManager,
            Supplier<@Nullable Tab> currentTabSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            WindowAndroid windowAndroid,
            Supplier<Toolbar> toolbarSupplier,
            @Nullable HomeSurfaceTracker homeSurfaceTracker,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            ObservableSupplier<Integer> tabStripHeightSupplier,
            OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            ObservableSupplier<TopInsetCoordinator> topInsetCoordinatorSupplier,
            StartupMetricsTracker startupMetricsTracker,
            @Nullable ExclusiveAccessManager exclusiveAccessManager,
            BackPressManager backPressManager,
            MultiInstanceManager multiInstanceManager) {
        mActivity = activity;
        mAppBrowserControlsVisibilityDelegate = appBrowserControlsVisibilityDelegate;
        mShareDelegateSupplier = shareDelegateSupplier;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mContextMenuCopyLinkObserver = contextMenuCopyLinkObserver;
        mBottomSheetController = sheetController;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
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
        mToolbarSupplier = toolbarSupplier;
        mHomeSurfaceTracker = homeSurfaceTracker;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mModuleRegistrySupplier = moduleRegistrySupplier;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mTopInsetCoordinatorSupplier = topInsetCoordinatorSupplier;
        mStartupMetricsTracker = startupMetricsTracker;
        mExclusiveAccessManager = exclusiveAccessManager;
        mBackPressManager = backPressManager;
        mMultiInstanceManager = multiInstanceManager;
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new ActivityTabWebContentsDelegateAndroid(
                tab,
                mActivity,
                mChromeActivityNativeDelegate,
                /* isCustomTab= */ false,
                mBrowserControlsStateProvider,
                mFullscreenManager,
                mTabCreatorManager,
                mTabModelSelectorSupplier,
                mCompositorViewHolderSupplier,
                mModalDialogManagerSupplier,
                mExclusiveAccessManager);
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return new ExternalNavigationHandler(new ExternalNavigationDelegateImpl(tab));
    }

    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        return new ChromeContextMenuPopulatorFactory(
                new TabContextMenuItemDelegate(
                        mActivity,
                        ActivityType.TABBED,
                        tab,
                        mTabModelSelectorSupplier.get(),
                        mEphemeralTabCoordinatorSupplier,
                        mContextMenuCopyLinkObserver,
                        mSnackbarManagerSupplier,
                        () -> mBottomSheetController,
                        mMultiInstanceManager),
                mShareDelegateSupplier,
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL,
                /* customContentActions= */ List.of());
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new ComposedBrowserControlsVisibilityDelegate(
                new TabStateBrowserControlsVisibilityDelegate(tab),
                mAppBrowserControlsVisibilityDelegate);
    }

    @Override
    public @Nullable NativePage createNativePage(
            String url, @Nullable NativePage candidatePage, Tab tab, @Nullable PdfInfo pdfInfo) {
        if (mNativePageFactory == null) {
            mNativePageFactory =
                    new NativePageFactory(
                            mActivity,
                            mBottomSheetController,
                            mBrowserControlsManager,
                            mCurrentTabSupplier,
                            mSnackbarManagerSupplier,
                            mLifecycleDispatcher,
                            mTabModelSelectorSupplier.get(),
                            mShareDelegateSupplier,
                            mWindowAndroid,
                            mToolbarSupplier,
                            mHomeSurfaceTracker,
                            mTabContentManagerSupplier,
                            mTabStripHeightSupplier,
                            mModuleRegistrySupplier,
                            mEdgeToEdgeControllerSupplier,
                            mTopInsetCoordinatorSupplier,
                            mStartupMetricsTracker,
                            mBackPressManager,
                            mMultiInstanceManager);
        }
        return mNativePageFactory.createNativePage(url, candidatePage, tab, pdfInfo);
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
        if (mNativePageFactory != null) mNativePageFactory.destroy();
    }
}
