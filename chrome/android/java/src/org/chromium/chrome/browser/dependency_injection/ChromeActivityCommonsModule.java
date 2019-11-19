// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.ui.base.ActivityWindowAndroid;

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
    public interface Factory { ChromeActivityCommonsModule create(ChromeActivity<?> activity); }

    public ChromeActivityCommonsModule(
            ChromeActivity<?> activity, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
    }

    @Provides
    public BottomSheetController provideBottomSheetController() {
        // Once the BottomSheetController is in the dependency graph, this method would no longer
        // be necessary, as well as the getter in ChromeActivity. Same is true for a few other
        // methods below.
        return mActivity.getBottomSheetController();
    }

    @Provides
    public TabModelSelector provideTabModelSelector() {
        return mActivity.getTabModelSelector();
    }

    @Provides
    public ChromeFullscreenManager provideChromeFullscreenManager() {
        return mActivity.getFullscreenManager();
    }

    @Provides
    public LayoutManager provideLayoutManager() {
        return mActivity.getCompositorViewHolder().getLayoutManager();
    }

    @Provides
    public ChromeActivity provideChromeActivity() {
        // Ideally providing Context or Activity should be enough, but currently a lot of code is
        // coupled specifically to ChromeActivity.
        return mActivity;
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
        return (TabCreatorManager) mActivity;
    }

    @Provides
    public StatusBarColorController provideStatusBarColorController() {
        return mActivity.getStatusBarColorController();
    }

    @Provides
    public ScreenOrientationProvider provideScreenOrientationProvider() {
        return ScreenOrientationProvider.getInstance();
    }
}
