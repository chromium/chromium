// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_TYPE;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import dagger.Module;
import dagger.Provides;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.metrics.LegacyTabStartupMetricsTracker;
import org.chromium.chrome.browser.metrics.StartupMetricsTracker;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelInitializer;
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

/** Module for common dependencies in {@link ChromeActivity}. */
@Module
public class ChromeActivityCommonsModule {
    private final AppCompatActivity mActivity;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final BrowserControlsManager mBrowserControlsManager;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final FullscreenManager mFullscreenManager;
    private final Supplier<LayoutManagerImpl> mLayoutManagerSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final ActivityTabProvider mActivityTabProvider;
    private final TabContentManager mTabContentManager;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<TabCreator> mTabCreatorSupplier;
    private final StatusBarColorController mStatusBarColorController;
    private final ScreenOrientationProvider mScreenOrientationProvider;
    private final Supplier<NotificationManagerProxy> mNotificationManagerProxySupplier;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final Supplier<LegacyTabStartupMetricsTracker> mLegacyTabStartupMetricsTrackerSupplier;
    private final Supplier<StartupMetricsTracker> mStartupMetricsTrackerSupplier;
    private final CompositorViewHolder.Initializer mCompositorViewHolderInitializer;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;
    private final ObservableSupplier<Integer> mAutofillUiBottomInsetSupplier;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final TabModelInitializer mTabModelInitializer;
    private final @ActivityType int mActivityType;

    /** See {@link ModuleFactoryOverrides} */
    public interface Factory {
        ChromeActivityCommonsModule create(
                AppCompatActivity activity,
                Supplier<BottomSheetController> bottomSheetControllerSupplier,
                Supplier<TabModelSelector> tabModelSelectorSupplier,
                BrowserControlsManager browserControlsManager,
                BrowserControlsVisibilityManager browserControlsVisibilityManager,
                BrowserControlsSizer browserControlsSizer,
                FullscreenManager fullscreenManager,
                Supplier<LayoutManagerImpl> layoutManagerSupplier,
                ActivityLifecycleDispatcher lifecycleDispatcher,
                Supplier<SnackbarManager> snackbarManagerSupplier,
                OneshotSupplier<ProfileProvider> profileProvider,
                ActivityTabProvider activityTabProvider,
                TabContentManager tabContentManager,
                ActivityWindowAndroid activityWindowAndroid,
                Supplier<CompositorViewHolder> compositorViewHolderSupplier,
                TabCreatorManager tabCreatorManager,
                Supplier<TabCreator> tabCreatorSupplier,
                StatusBarColorController statusBarColorController,
                ScreenOrientationProvider screenOrientationProvider,
                Supplier<NotificationManagerProxy> notificationManagerProxySupplier,
                ObservableSupplier<TabContentManager> tabContentManagerSupplier,
                Supplier<LegacyTabStartupMetricsTracker> legacyTabStartupMetricsTrackerSupplier,
                Supplier<StartupMetricsTracker> startupMetricsTrackerSupplier,
                CompositorViewHolder.Initializer compositorViewHolderInitializer,
                ChromeActivityNativeDelegate chromeActivityNativeDelegate,
                Supplier<ModalDialogManager> modalDialogManagerSupplier,
                BrowserControlsStateProvider browserControlsStateProvider,
                Supplier<Bundle> savedInstanceStateSupplier,
                ObservableSupplier<Integer> autofillUiBottomInsetSupplier,
                Supplier<ShareDelegate> shareDelegateSupplier,
                TabModelInitializer tabModelInitializer,
                @ActivityType int activityType);
    }

    public ChromeActivityCommonsModule(
            AppCompatActivity activity,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            BrowserControlsManager browserControlsManager,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            BrowserControlsSizer browserControlsSizer,
            FullscreenManager fullscreenManager,
            Supplier<LayoutManagerImpl> layoutManagerSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            ActivityTabProvider activityTabProvider,
            TabContentManager tabContentManager,
            ActivityWindowAndroid activityWindowAndroid,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            TabCreatorManager tabCreatorManager,
            Supplier<TabCreator> tabCreatorSupplier,
            StatusBarColorController statusBarColorController,
            ScreenOrientationProvider screenOrientationProvider,
            Supplier<NotificationManagerProxy> notificationManagerProxySupplier,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<LegacyTabStartupMetricsTracker> legacyTabStartupMetricsTrackerSupplier,
            Supplier<StartupMetricsTracker> startupMetricsTrackerSupplier,
            CompositorViewHolder.Initializer compositorViewHolderInitializer,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<Bundle> savedInstanceStateSupplier,
            ObservableSupplier<Integer> autofillUiBottomInsetSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            TabModelInitializer tabModelInitializer,
            @ActivityType int activityType) {
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
        mProfileProviderSupplier = profileProviderSupplier;
        mActivityTabProvider = activityTabProvider;
        mTabContentManager = tabContentManager;
        mActivityWindowAndroid = activityWindowAndroid;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mTabCreatorManager = tabCreatorManager;
        mTabCreatorSupplier = tabCreatorSupplier;
        mStatusBarColorController = statusBarColorController;
        mScreenOrientationProvider = screenOrientationProvider;
        mNotificationManagerProxySupplier = notificationManagerProxySupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mLegacyTabStartupMetricsTrackerSupplier = legacyTabStartupMetricsTrackerSupplier;
        mStartupMetricsTrackerSupplier = startupMetricsTrackerSupplier;
        mCompositorViewHolderInitializer = compositorViewHolderInitializer;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
        mAutofillUiBottomInsetSupplier = autofillUiBottomInsetSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
        mTabModelInitializer = tabModelInitializer;
        mActivityType = activityType;
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
    @Named(ACTIVITY_CONTEXT)
    public Context provideContext() {
        return mActivity;
    }

    @Provides
    public Activity provideActivity() {
        return mActivity;
    }

    @Provides
    public AppCompatActivity provideAppCompatActivity() {
        return mActivity;
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
    public OneshotSupplier<ProfileProvider> provideProfileProviderSupplier() {
        return mProfileProviderSupplier;
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
    public WindowAndroid provideWindowAndroid() {
        return mActivityWindowAndroid;
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
    public LegacyTabStartupMetricsTracker provideLegacyTabStartupMetricsTracker() {
        return mLegacyTabStartupMetricsTrackerSupplier.get();
    }

    @Provides
    public StartupMetricsTracker provideStartupMetricsTracker() {
        return mStartupMetricsTrackerSupplier.get();
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

    @Provides
    public ObservableSupplier<Integer> provideAutofillUiBottomInsetSupplier() {
        return mAutofillUiBottomInsetSupplier;
    }

    @Provides
    public Supplier<ShareDelegate> provideShareDelegateSupplier() {
        return mShareDelegateSupplier;
    }

    @Provides
    public TabModelInitializer provideTabModelInitializer() {
        return mTabModelInitializer;
    }

    @Provides
    @Named(ACTIVITY_TYPE)
    public @ActivityType int provideActivityType() {
        return mActivityType;
    }
}
