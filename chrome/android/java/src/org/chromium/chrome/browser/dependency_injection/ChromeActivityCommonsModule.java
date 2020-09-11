// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.DECOR_VIEW;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.IS_PROMOTABLE_TO_TAB_BOOLEAN;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import javax.inject.Named;

import dagger.Module;
import dagger.Provides;

/**
 * Module for common dependencies in {@link ChromeActivity}.
 */
@Module
public class ChromeActivityCommonsModule {
    private final ChromeActivity<?> mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    /** See {@link ModuleFactoryOverrides} */
    public interface Factory {
        ChromeActivityCommonsModule create(ChromeActivity<?> activity,
                ActivityLifecycleDispatcher activityLifecycleDispatcher);
    }

    public ChromeActivityCommonsModule(
            ChromeActivity<?> activity, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
    }

    @Provides
    public BottomSheetController provideBottomSheetController() {
        return BottomSheetControllerProvider.from(mActivity.getWindowAndroid());
    }

    @Provides
    public TabModelSelector provideTabModelSelector() {
        return mActivity.getTabModelSelector();
    }

    @Provides
    public BrowserControlsManager provideBrowserControlsManager() {
        return mActivity.getBrowserControlsManager();
    }

    @Provides
    public BrowserControlsVisibilityManager provideBrowserControlsVisibilityManager() {
        return mActivity.getBrowserControlsManager();
    }

    @Provides
    public BrowserControlsSizer provideBrowserControlsSizer() {
        return mActivity.getBrowserControlsManager();
    }

    @Provides
    public FullscreenManager provideFullscreenManager() {
        return mActivity.getFullscreenManager();
    }

    @Provides
    public LayoutManager provideLayoutManager() {
        return mActivity.getCompositorViewHolder().getLayoutManager();
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
        return mActivity.getSnackbarManager();
    }

    @Provides
    public ActivityTabProvider provideActivityTabProvider() {
        return mActivity.getActivityTabProvider();
    }

    @Provides
    public TabContentManager provideTabContentManager() {
        return mActivity.getTabContentManager();
    }

    @Provides
    public ActivityWindowAndroid provideActivityWindowAndroid() {
        return mActivity.getWindowAndroid();
    }

    @Provides
    public CompositorViewHolder provideCompositorViewHolder() {
        return mActivity.getCompositorViewHolder();
    }

    @Provides
    public TabCreatorManager provideTabCreatorManager() {
        return mActivity;
    }

    @Provides
    public Supplier<TabCreator> provideTabCreator() {
        return mActivity::getCurrentTabCreator;
    }

    @Provides
    @Named(IS_PROMOTABLE_TO_TAB_BOOLEAN)
    public boolean provideIsPromotableToTab() {
        return !mActivity.isCustomTab();
    }

    @Provides
    public StatusBarColorController provideStatusBarColorController() {
        return mActivity.getStatusBarColorController();
    }

    @Provides
    public ScreenOrientationProvider provideScreenOrientationProvider() {
        return ScreenOrientationProvider.getInstance();
    }

    @Provides
    public NotificationManagerProxy provideNotificationManagerProxy() {
        return new NotificationManagerProxyImpl(mActivity.getApplicationContext());
    }
}
