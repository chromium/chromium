// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

import java.util.function.Supplier;

/** Class that holds the {@link CustomTabMinimizationManager}. */
@NullMarked
public class CustomTabMinimizationManagerHolder implements DestroyObserver {
    private final AppCompatActivity mActivity;
    private final Supplier<CustomTabActivityNavigationController> mNavigationController;
    private final ActivityTabProvider mActivityTabProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;

    private @Nullable MinimizedCustomTabIphController mIphController;
    private @Nullable CustomTabMinimizationManager mMinimizationManager;

    public CustomTabMinimizationManagerHolder(
            AppCompatActivity activity,
            Supplier<CustomTabActivityNavigationController> navigationController,
            ActivityTabProvider activityTabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            Supplier<Bundle> savedInstanceStateSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mNavigationController = navigationController;
        mActivityTabProvider = activityTabProvider;
        mIntentDataProvider = intentDataProvider;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
        mLifecycleDispatcher = lifecycleDispatcher;

        mLifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        destroyMinimizationManager();
    }

    public void maybeCreateMinimizationManager(ObservableSupplier<Profile> profileSupplier) {
        if (MinimizedFeatureUtils.isMinimizedCustomTabAvailable(mActivity)
                && MinimizedFeatureUtils.shouldEnableMinimizedCustomTabs(mIntentDataProvider)) {
            mIphController =
                    new MinimizedCustomTabIphController(
                            mActivity,
                            mActivityTabProvider,
                            new UserEducationHelper(
                                    mActivity,
                                    (Supplier<@Nullable Profile>) profileSupplier,
                                    new Handler(Looper.getMainLooper())),
                            profileSupplier);
            Runnable closeTabRunnable = mNavigationController.get()::navigateOnClose;
            mMinimizationManager =
                    new CustomTabMinimizationManager(
                            mActivity,
                            mActivityTabProvider,
                            mIphController,
                            closeTabRunnable,
                            mIntentDataProvider,
                            mLifecycleDispatcher,
                            mSavedInstanceStateSupplier);
        }
    }

    public @Nullable CustomTabMinimizationManager getMinimizationManager() {
        return mMinimizationManager;
    }

    private void destroyMinimizationManager() {
        if (mMinimizationManager != null) {
            mMinimizationManager.destroy();
            mMinimizationManager = null;
        }

        if (mIphController != null) {
            mIphController.destroy();
            mIphController = null;
        }
    }
}
