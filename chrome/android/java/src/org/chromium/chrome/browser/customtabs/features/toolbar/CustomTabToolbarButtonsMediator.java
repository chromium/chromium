// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MINIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.OPTIONAL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TOOLBAR_WIDTH;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Handler;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabFeatureOverridesManager;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.OnNewWidthMeasuredListener;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MinimizeButtonData;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class CustomTabToolbarButtonsMediator
        implements OnNewWidthMeasuredListener,
                ConfigurationChangedObserver,
                CustomTabToolbar.OnColorSchemeChangedObserver {
    private final PropertyModel mModel;
    private final CustomTabToolbar mView;
    private final Activity mActivity;
    private final CustomTabMinimizeDelegate mMinimizeDelegate;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final ActivityTabProvider mTabProvider;

    /** Whether the minimize button is available for the device and the current configuration. */
    private final boolean mMinimizeButtonAvailable;

    private boolean mMinimizeButtonEnabled;
    private @Nullable OptionalButtonCoordinator mOptionalButtonCoordinator;

    CustomTabToolbarButtonsMediator(
            PropertyModel model,
            CustomTabToolbar view,
            Activity activity,
            CustomTabMinimizeDelegate minimizeDelegate,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabFeatureOverridesManager featureOverridesManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ActivityTabProvider tabProvider) {
        mModel = model;
        mView = view;
        mActivity = activity;
        mMinimizeButtonAvailable =
                getMinimizeButtonAvailable(
                        activity, minimizeDelegate, intentDataProvider, featureOverridesManager);
        mMinimizeDelegate = minimizeDelegate;
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mMinimizeButtonEnabled = true;
        mTabProvider = tabProvider;

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

    @Override
    public void onColorSchemeChanged(
            @ColorInt int toolbarColor, @BrandedColorScheme int colorScheme) {
        updateOptionalButtonColors(toolbarColor, colorScheme);
    }

    void setMinimizeButtonEnabled(boolean enabled) {
        mMinimizeButtonEnabled = enabled;
        mModel.set(MINIMIZE_BUTTON, getMinimizeButtonData());
    }

    void setOptionalButtonData(@Nullable ButtonData buttonData) {
        if (buttonData != null && mOptionalButtonCoordinator == null) {
            mOptionalButtonCoordinator =
                    new OptionalButtonCoordinator(
                            mView.ensureOptionalButtonInflated(),
                            /* userEducationHelper= */ () -> {
                                Tab currentTab = mTabProvider.get();
                                assert currentTab != null;
                                return new UserEducationHelper(
                                        assumeNonNull(
                                                currentTab
                                                        .getWindowAndroidChecked()
                                                        .getActivity()
                                                        .get()),
                                        currentTab::getProfile,
                                        new Handler());
                            },
                            /* transitionRoot= */ mView,
                            /* isAnimationAllowedPredicate= */ () -> true,
                            /* featureEngagementTrackerSupplier= */ () ->
                                    TrackerFactory.getTrackerForProfile(
                                            assumeNonNull(mTabProvider.get()).getProfile()));
            int width =
                    mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
            mOptionalButtonCoordinator.setCollapsedStateWidth(width);
        }

        if (mOptionalButtonCoordinator != null) {
            mOptionalButtonCoordinator.updateButton(buttonData, mModel.get(IS_INCOGNITO));
            updateOptionalButtonColors(
                    mView.getBackground().getColor(), mView.getBrandedColorScheme());

            mModel.set(OPTIONAL_BUTTON_VISIBLE, buttonData != null && buttonData.canShow());
        }
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

    private void updateOptionalButtonColors(
            @ColorInt int toolbarColor, @BrandedColorScheme int colorScheme) {
        if (mOptionalButtonCoordinator == null) return;

        int backgroundColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        mActivity,
                        toolbarColor,
                        colorScheme == BrandedColorScheme.INCOGNITO,
                        /* isCustomTab= */ true);
        mOptionalButtonCoordinator.setBackgroundColorFilter(backgroundColor);
        mOptionalButtonCoordinator.setIconForegroundColor(
                ThemeUtils.getThemedToolbarIconTint(mActivity, colorScheme));
    }
}
