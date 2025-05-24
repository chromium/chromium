// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MINIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TOOLBAR_WIDTH;

import android.app.Activity;
import android.content.res.Configuration;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabFeatureOverridesManager;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.OnNewWidthMeasuredListener;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MinimizeButtonData;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class CustomTabToolbarButtonsMediator
        implements OnNewWidthMeasuredListener, ConfigurationChangedObserver {
    private final PropertyModel mModel;
    private final Activity mActivity;
    private final CustomTabMinimizeDelegate mMinimizeDelegate;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    /** Whether the minimize button is available for the device and the current configuration. */
    private final boolean mMinimizeButtonAvailable;

    private boolean mMinimizeButtonEnabled;

    CustomTabToolbarButtonsMediator(
            PropertyModel model,
            Activity activity,
            CustomTabMinimizeDelegate minimizeDelegate,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabFeatureOverridesManager featureOverridesManager,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mModel = model;
        mActivity = activity;
        mMinimizeButtonAvailable =
                getMinimizeButtonAvailable(
                        activity, minimizeDelegate, intentDataProvider, featureOverridesManager);
        mMinimizeDelegate = minimizeDelegate;
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mMinimizeButtonEnabled = true;

        // Set the initial real minimize button data.
        mModel.set(MINIMIZE_BUTTON, getMinimizeButtonData());
    }

    public void destroy() {
        mLifecycleDispatcher.unregister(this);
    }

    @Override
    public void onNewWidthMeasured(int width) {
        mModel.set(TOOLBAR_WIDTH, width);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        mModel.set(MINIMIZE_BUTTON, getMinimizeButtonData());
    }

    void setMinimizeButtonEnabled(boolean enabled) {
        mMinimizeButtonEnabled = enabled;
        mModel.set(MINIMIZE_BUTTON, getMinimizeButtonData());
    }

    private MinimizeButtonData getMinimizeButtonData() {
        boolean isMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
        return new MinimizeButtonData(
                mMinimizeButtonAvailable && mMinimizeButtonEnabled && !isMultiWindow,
                v -> {
                    if (mMinimizeDelegate != null) mMinimizeDelegate.minimize();
                });
    }

    private static boolean getMinimizeButtonAvailable(
            Activity activity,
            CustomTabMinimizeDelegate minimizeDelegate,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabFeatureOverridesManager featureOverridesManager) {
        return MinimizedFeatureUtils.isMinimizedCustomTabAvailable(
                        activity, featureOverridesManager)
                && MinimizedFeatureUtils.shouldEnableMinimizedCustomTabs(intentDataProvider)
                && minimizeDelegate != null;
    }
}
