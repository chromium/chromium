// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MINIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.OPTIONAL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TOOLBAR_WIDTH;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Handler;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.OnNewWidthMeasuredListener;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MinimizeButtonData;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator.TransitionType;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.Tracker;
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
    private final ObservableSupplierImpl<Tracker> mTrackerSupplier = new ObservableSupplierImpl<>();

    CustomTabToolbarButtonsMediator(
            PropertyModel model,
            CustomTabToolbar view,
            Activity activity,
            CustomTabMinimizeDelegate minimizeDelegate,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ActivityTabProvider tabProvider) {
        mModel = model;
        mView = view;
        mActivity = activity;
        mMinimizeButtonAvailable =
                getMinimizeButtonAvailable(activity, minimizeDelegate, intentDataProvider);
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
            View optionalButton = mView.ensureOptionalButtonInflated();
            mOptionalButtonCoordinator =
                    new OptionalButtonCoordinator(
                            optionalButton,
                            () ->
                                    new UserEducationHelper(
                                            mActivity, getProfileSupplier(), new Handler()),
                            mView,
                            /* isAnimationAllowedPredicate= */ () -> true,
                            mTrackerSupplier);
            int width =
                    mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
            mOptionalButtonCoordinator.setCollapsedStateWidth(width);
            mOptionalButtonCoordinator.setTransitionFinishedCallback(
                    transitionType -> {
                        switch (transitionType) {
                            case TransitionType.EXPANDING_ACTION_CHIP:
                                // TODO(crbug.com/428261559): Adjust URL/Title bar width accordingly
                                break;
                        }
                        mView.requestLayout();
                    });
            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CCT_ADAPTIVE_BUTTON_TEST_SWITCH, "always-animate", false)) {
                mOptionalButtonCoordinator.setAlwaysShowActionChip(true);
            }
            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CCT_ADAPTIVE_BUTTON_TEST_SWITCH, "hide-button", false)) {
                // TODO(crbug.com/428261559): Simulate the shortened screen width to hide MTB.
            }
        }

        if (mOptionalButtonCoordinator != null) {
            if (buttonData != null) {
                Tab tab = mTabProvider.get();
                if (tab != null && mTrackerSupplier.get() == null) {
                    mTrackerSupplier.set(TrackerFactory.getTrackerForProfile(tab.getProfile()));
                }
                mOptionalButtonCoordinator.updateButton(buttonData, mModel.get(IS_INCOGNITO));
                updateOptionalButtonColors(
                        mView.getBackground().getColor(), mView.getBrandedColorScheme());
            }
            mModel.set(OPTIONAL_BUTTON_VISIBLE, buttonData != null && buttonData.canShow());
        }
    }

    @SuppressWarnings("NullAway")
    private Supplier getProfileSupplier() {
        Tab tab = mTabProvider.get();
        if (tab != null) return () -> tab.getProfile();

        // Passing OneshotSupplier effectively delays UserEducationHelper#requestShowIph()
        // till Profile becomes reachable via the current Tab.
        var profileSupplier = new OneshotSupplierImpl<Profile>();
        mTabProvider.addSyncObserver(
                new Callback<@Nullable Tab>() {
                    @Override
                    public void onResult(@Nullable Tab currentTab) {
                        if (currentTab == null) return;

                        mTabProvider.removeObserver(this);
                        profileSupplier.set(currentTab.getProfile());
                    }
                });
        return profileSupplier;
    }

    boolean isOptionalButtonVisible() {
        return mModel.get(OPTIONAL_BUTTON_VISIBLE);
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
            BrowserServicesIntentDataProvider intentDataProvider) {
        return MinimizedFeatureUtils.isMinimizedCustomTabAvailable(activity)
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
