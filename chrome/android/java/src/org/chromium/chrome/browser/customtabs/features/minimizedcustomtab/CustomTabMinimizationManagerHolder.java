// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

import javax.inject.Inject;

/** Class that holds the {@link CustomTabMinimizationManager}. */
@ActivityScope
public class CustomTabMinimizationManagerHolder implements DestroyObserver {
    private final AppCompatActivity mActivity;
    private final CustomTabActivityNavigationController mNavigationController;
    private final ActivityTabProvider mActivityTabProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    private @Nullable MinimizedCustomTabIPHController mIPHController;
    private @Nullable CustomTabMinimizationManager mMinimizationManager;

    @Inject
    public CustomTabMinimizationManagerHolder(
            AppCompatActivity activity,
            CustomTabActivityNavigationController navigationController,
            ActivityTabProvider activityTabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mNavigationController = navigationController;
        mActivityTabProvider = activityTabProvider;
        mIntentDataProvider = intentDataProvider;

        lifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        destroyMinimizationManager();
    }

    public void maybeCreateMinimizationManager(ObservableSupplier<Profile> profileSupplier) {
        if (MinimizedFeatureUtils.isMinimizedCustomTabAvailable(mActivity)) {
            mIPHController =
                    new MinimizedCustomTabIPHController(
                            mActivity,
                            mActivityTabProvider,
                            new UserEducationHelper(mActivity, new Handler(Looper.getMainLooper())),
                            profileSupplier);
            Runnable closeTabRunnable = mNavigationController::navigateOnClose;
            // The method above already checks for the minimum API level.
            //
            // noinspection NewApi
            mMinimizationManager =
                    new CustomTabMinimizationManager(
                            mActivity,
                            mActivityTabProvider,
                            mIPHController,
                            closeTabRunnable,
                            mIntentDataProvider);
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

        if (mIPHController != null) {
            mIPHController.destroy();
            mIPHController = null;
        }
    }
}
