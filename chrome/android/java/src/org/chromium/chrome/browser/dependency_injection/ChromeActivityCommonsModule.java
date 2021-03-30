// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.DECOR_VIEW;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.IS_PROMOTABLE_TO_TAB_BOOLEAN;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.metrics.ActivityTabStartupMetricsTracker;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import javax.inject.Named;

import dagger.Module;
import dagger.Provides;

/**
 * Module for common dependencies in {@link ChromeActivity}.
 */
@Module
public class ChromeActivityCommonsModule {
    private final ChromeActivity mActivity;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final BrowserControlsManager mBrowserControlsManager;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final FullscreenManager mFullscreenManager;
    private final Supplier<LayoutManagerImpl> mLayoutManagerSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final ActivityTabProvider mActivityTabProvider;
    private final TabContentManager mTabContentManager;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<TabCreator> mTabCreatorSupplier;
    private final Supplier<Boolean> mIsPromotableToTabSupplier;
    private final StatusBarColorController mStatusBarColorController;
    private final ScreenOrientationProvider mScreenOrientationProvider;
    private final Supplier<NotificationManagerProxy> mNotificationManagerProxySupplier;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final Supplier<ActivityTabStartupMetricsTracker>
            mActivityTabStartupMetricsTrackerSupplier;
    private final CompositorViewHolder.Initializer mCompositorViewHolderInitializer;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;

    /** See {@link ModuleFactoryOverrides} */
    public interface Factory {
        ChromeActivityCommonsModule create(ChromeActivity activity,
                Supplier<BottomSheetController> bottomSheetControllerSupplier,
                Supplier<TabModelSelector> tabModelSelectorSupplier,
                BrowserControlsManager browserControlsManager,
                BrowserControlsVisibilityManager browserControlsVisibilityManager,
                BrowserControlsSizer browserControlsSizer, FullscreenManager fullscreenManager,
                Supplier<LayoutManagerImpl> layoutManagerSupplier,
                ActivityLifecycleDispatcher lifecycleDispatcher,
                Supplier<SnackbarManager> snackbarManagerSupplier,
                ActivityTabProvider activityTabProvider, TabContentManager tabContentManager,
                ActivityWindowAndroid activityWindowAndroid,
                Supplier<CompositorViewHolder> compositorViewHolderSupplier,
                TabCreatorManager tabCreatorManager, Supplier<TabCreator> tabCreatorSupplier,
                Supplier<Boolean> isPromotableToTabSupplier,
                StatusBarColorController statusBarColorController,
                ScreenOrientationProvider screenOrientationProvider,
                Supplier<NotificationManagerProxy> notificationManagerProxySupplier,
                ObservableSupplier<TabContentManager> tabContentManagerSupplier,
                Supplier<ActivityTabStartupMetricsTracker> activityTabStartupMetricsTrackerSupplier,
                CompositorViewHolder.Initializer compositorViewHolderInitializer,
                ChromeActivityNativeDelegate chromeActivityNativeDelegate,
                Supplier<ModalDialogManager> modalDialogManagerSupplier,
                BrowserControlsStateProvider browserControlsStateProvider,
                Supplier<Bundle> savedInstanceStateSupplier);
    }

    public ChromeActivityCommonsModule(ChromeActivity activity,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            BrowserControlsManager browserControlsManager,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            BrowserControlsSizer browserControlsSizer, FullscreenManager fullscreenManager,
            Supplier<LayoutManagerImpl> layoutManagerSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            ActivityTabProvider activityTabProvider, TabContentManager tabContentManager,
            ActivityWindowAndroid activityWindowAndroid,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            TabCreatorManager tabCreatorManager, Supplier<TabCreator> tabCreatorSupplier,
            Supplier<Boolean> isPromotableToTabSupplier,
            StatusBarColorController statusBarColorController,
            ScreenOrientationProvider screenOrientationProvider,
            Supplier<NotificationManagerProxy> notificationManagerProxySupplier,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<ActivityTabStartupMetricsTracker> activityTabStartupMetricsTrackerSupplier,
            CompositorViewHolder.Initializer compositorViewHolderInitializer,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<Bundle> savedInstanceStateSupplier) {
        mActivity = activity;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mBrowserControlsManager = browserControlsManager;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mBrowserControlsSizer = browserControlsSizer;
        mFullscreenManager = fullscreenManager;
        mLayoutManagerSupplier = layoutManagerSupplier;
        mLifecycleDispatcher = lifecycleDispatcher;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mActivityTabProvider = activityTabProvider;
        mTabContentManager = tabContentManager;
        mActivityWindowAndroid = activityWindowAndroid;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mTabCreatorManager = tabCreatorManager;
        mTabCreatorSupplier = tabCreatorSupplier;
        mIsPromotableToTabSupplier = isPromotableToTabSupplier;
        mStatusBarColorController = statusBarColorController;
        mScreenOrientationProvider = screenOrientationProvider;
        mNotificationManagerProxySupplier = notificationManagerProxySupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mActivityTabStartupMetricsTrackerSupplier = activityTabStartupMetricsTrackerSupplier;
        mCompositorViewHolderInitializer = compositorViewHolderInitializer;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
    }

    @Provides
    public BottomSheetController provideBottomSheetController() {
        return mBottomSheetControllerSupplier.get();
    }

    @Provides
    public TabModelSelector provideTabModelSelector() {
        return mTabModelSelectorSupplier.get();
    }

    @Provides
    public Supplier<TabModelSelector> provideTabModelSelectorSupplier() {
        return mTabModelSelectorSupplier;
    }

    @Provides
    public BrowserControlsManager provideBrowserControlsManager() {
        return mBrowserControlsManager;
    }

    @Provides
    public BrowserControlsVisibilityManager provideBrowserControlsVisibilityManager() {
        return mBrowserControlsVisibilityManager;
    }

    @Provides
    public BrowserControlsSizer provideBrowserControlsSizer() {
        return mBrowserControlsSizer;
    }

    @Provides
    public FullscreenManager provideFullscreenManager() {
        return mFullscreenManager;
    }

    @Provides
    public LayoutManagerImpl provideLayoutManager() {
        return mLayoutManagerSupplier.get();
    }

    @Provides
    public ChromeActivity<?> provideChromeActivity() {
        // Ideally providing Context or Activity should be enough, but currently a lot of code is
        // coupled specifically to ChromeActivity.
        return mActivity;
    }

    @Provides
    public WindowAndroid provideWindowAndroid() {
        return mActivity.getWindowAndroid();
    }

    @Provides
    @Named(ACTIVITY_CONTEXT)
    public Context provideContext() {
        return mActivity;
    }

    @Provides
    public Activity provideActivity() {
        return mActivity;
    }

    @Provides
    @Named(DECOR_VIEW)
    public View provideDecorView() {
        return mActivity.getWindow().getDecorView();
    }

    @Provides
    public Resources provideResources() {
        return mActivity.getResources();
    }

    @Provides
    public ActivityLifecycleDispatcher provideLifecycleDispatcher() {
        return mLifecycleDispatcher;
    }

    @Provides
    public SnackbarManager provideSnackbarManager() {
        return mSnackbarManagerSupplier.get();
    }

    @Provides
    public ActivityTabProvider provideActivityTabProvider() {
        return mActivityTabProvider;
    }

    @Provides
    public TabContentManager provideTabContentManager() {
        return mTabContentManager;
    }

    @Provides
    public ActivityWindowAndroid provideActivityWindowAndroid() {
        return mActivityWindowAndroid;
    }

    @Provides
    public CompositorViewHolder provideCompositorViewHolder() {
        return mCompositorViewHolderSupplier.get();
    }

    @Provides
    public Supplier<CompositorViewHolder> provideCompositorViewHolderSupplier() {
        return mCompositorViewHolderSupplier;
    }

    @Provides
    public TabCreatorManager provideTabCreatorManager() {
        return mTabCreatorManager;
    }

    @Provides
    public Supplier<TabCreator> provideTabCreatorSupplier() {
        return mTabCreatorSupplier;
    }

    @Provides
    @Named(IS_PROMOTABLE_TO_TAB_BOOLEAN)
    public boolean provideIsPromotableToTab() {
        return !mIsPromotableToTabSupplier.get();
    }

    @Provides
    public StatusBarColorController provideStatusBarColorController() {
        return mStatusBarColorController;
    }

    @Provides
    public ScreenOrientationProvider provideScreenOrientationProvider() {
        return mScreenOrientationProvider;
    }

    @Provides
    public NotificationManagerProxy provideNotificationManagerProxy() {
        return mNotificationManagerProxySupplier.get();
    }

    @Provides
    public ObservableSupplier<TabContentManager> provideTabContentManagerSupplier() {
        return mTabContentManagerSupplier;
    }

    @Provides
    public ActivityTabStartupMetricsTracker provideActivityTabStartupMetricsTracker() {
        return mActivityTabStartupMetricsTrackerSupplier.get();
    }

    @Provides
    public CompositorViewHolder.Initializer provideCompositorViewHolderInitializer() {
        return mCompositorViewHolderInitializer;
    }

    @Provides
    public Supplier<ModalDialogManager> provideModalDialogManagerSupplier() {
        return mModalDialogManagerSupplier;
    }

    @Provides
    public ChromeActivityNativeDelegate provideChromeActivityNativeDelegate() {
        return mChromeActivityNativeDelegate;
    }

    @Provides
    public BrowserControlsStateProvider provideBrowserControlsStateProvider() {
        return mBrowserControlsStateProvider;
    }

    @Provides
    @Named(SAVED_INSTANCE_SUPPLIER)
    public Supplier<Bundle> savedInstanceStateSupplier() {
        return mSavedInstanceStateSupplier;
    }
}
