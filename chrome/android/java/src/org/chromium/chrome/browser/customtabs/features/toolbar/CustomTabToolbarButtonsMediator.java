// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.DESCRIPTION;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.ICON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MINIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.OMNIBOX_ENABLED;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.OPTIONAL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TINT;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TITLE_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TOOLBAR_WIDTH;

import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
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
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator.TransitionType;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

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
        var tint = ThemeUtils.getThemedToolbarIconTint(mActivity, colorScheme);
        mModel.set(TINT, tint);
    }

    void setMinimizeButtonEnabled(boolean enabled) {
        mMinimizeButtonEnabled = enabled;
        mModel.set(MINIMIZE_BUTTON, getMinimizeButtonData());
    }

    void updateCustomActionButton(int index, Drawable drawable, String description) {
        var model = mModel.get(CUSTOM_ACTION_BUTTONS).get(index);
        model.set(ICON, drawable);
        model.set(DESCRIPTION, description);
    }

    void setOptionalButtonData(@Nullable ButtonData buttonData) {
        boolean showOptionalButton =
                mOptionalButtonCoordinator == null ? initializeOptionalButton() : true;
        if (!showOptionalButton) return;

        if (buttonData == null) {
            if (mOptionalButtonCoordinator != null
                    && mOptionalButtonCoordinator.getViewVisibility() != View.GONE) {
                mOptionalButtonCoordinator.hideButton();
            }
            mModel.set(OPTIONAL_BUTTON_VISIBLE, false);
            return;
        }

        Tab tab = mTabProvider.get();
        if (tab != null && mTrackerSupplier.get() == null) {
            mTrackerSupplier.set(TrackerFactory.getTrackerForProfile(tab.getProfile()));
        }
        // Actual button update task is posted here, because the |Model#set()| below
        // triggers another round of toolbar positioning job that could interfere with
        // the button chip animation if done synchronously.
        new Handler().post(() -> updateOptionalButton(buttonData));
        mModel.set(OPTIONAL_BUTTON_VISIBLE, buttonData.canShow());
    }

    // Display a (blue) dot on the overflow menu icon for the optional button that cannot be
    // shown on the toolbar to indicate that the action is available through the menu.
    private void maybeShowActionMenuIndicator(@AdaptiveToolbarButtonVariant int buttonVariant) {
        if (mModel.get(OMNIBOX_ENABLED)) return;

        mView.setUpOptionalButtonFallbackUi(buttonVariant);
    }

    private int getCustomActionButtonCount() {
        return mModel.get(CUSTOM_ACTION_BUTTONS_VISIBLE)
                ? mModel.get(CUSTOM_ACTION_BUTTONS).size()
                : 0;
    }

    private void updateOptionalButton(ButtonData buttonData) {
        assertNonNull(mOptionalButtonCoordinator);

        if (!mModel.get(OPTIONAL_BUTTON_VISIBLE)) return;

        // By now inflation/positioning already has been done with OPTIONAL_BUTTON_VISIBLE as true.
        // If the visibility is false, it should be because the toolbar width is not big enough to
        // show it.
        int buttonVariant = buttonData.getButtonSpec().getButtonVariant();
        if (assumeNonNull(mView.getOptionalButton()).getVisibility() == View.GONE) {
            // See if we should show an indicator (a dot) if optional button cannot be shown.
            // This check needs to be invoked _after_ optional button initialization is
            // attempted, in order to determine its visibility in case it gets hidden due to
            // toolbar width/button count constraints.
            maybeShowActionMenuIndicator(buttonVariant);
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.AdaptiveToolbarButton.HiddenReason",
                    CustomTabMtbHiddenReason.TOOLBAR_WIDTH_LIMIT,
                    CustomTabMtbHiddenReason.COUNT);
        } else {
            mOptionalButtonCoordinator.updateButton(buttonData, mModel.get(IS_INCOGNITO));
            updateOptionalButtonColors(
                    mView.getBackground().getColor(), mView.getBrandedColorScheme());
            mView.setOptionalButtonBackgroundInset();
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.AdaptiveToolbarButton.Shown",
                    buttonVariant,
                    AdaptiveToolbarButtonVariant.MAX_VALUE);
        }
    }

    private boolean initializeOptionalButton() {
        assert mOptionalButtonCoordinator == null;

        if (getCustomActionButtonCount() >= 2) {
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.AdaptiveToolbarButton.HiddenReason",
                    CustomTabMtbHiddenReason.NO_BUTTON_SPACE,
                    CustomTabMtbHiddenReason.COUNT);
            return false;
        }
        if (mModel.get(OMNIBOX_ENABLED)) {
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.AdaptiveToolbarButton.HiddenReason",
                    CustomTabMtbHiddenReason.OMNIBOX_ENABLED,
                    CustomTabMtbHiddenReason.COUNT);
            return false;
        }
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
        int width = mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        mOptionalButtonCoordinator.setCollapsedStateWidth(width);
        mOptionalButtonCoordinator.setOnBeforeWidthTransitionCallback(
                (type, widthDelta) -> {
                    // As CPA chip does the expansion animation, the custom action button on
                    // the left of it needs animating too. Adjusting its margin makes it
                    // animate together with the chip.
                    var view = assumeNonNull(mView.getCustomActionButtonsParent()).getChildAt(0);
                    var viewLp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
                    if (type == TransitionType.EXPANDING_ACTION_CHIP
                            || type == TransitionType.COLLAPSING_ACTION_CHIP) {
                        viewLp.setMarginEnd(viewLp.rightMargin + widthDelta);
                        view.setLayoutParams(viewLp);
                    }
                });
        int[] orgMargin = new int[1]; // URL bar right margin.
        mOptionalButtonCoordinator.setTransitionFinishedCallback(
                type -> {
                    var locationBar = mView.findViewById(R.id.location_bar_frame_layout);
                    var locationBarLp =
                            ((ViewGroup.MarginLayoutParams) locationBar.getLayoutParams());
                    if (type == TransitionType.EXPANDING_ACTION_CHIP) {
                        // Cache the original margin of the URL bar before chip expansion. Will be
                        // used to restore the bar after the animation is finished.
                        orgMargin[0] = locationBarLp.rightMargin;
                        // Increase URL bar margin by the expanded CPA chip width i.e.
                        // |new button width| - |org button width|
                        int increasedMargin =
                                assumeNonNull(mOptionalButtonCoordinator).getViewWidth() - width;
                        locationBarLp.setMarginEnd(locationBarLp.rightMargin + increasedMargin);
                        locationBar.setLayoutParams(locationBarLp);
                    } else if (type == TransitionType.COLLAPSING_ACTION_CHIP) {
                        locationBarLp.setMarginEnd(orgMargin[0]);
                        locationBar.setLayoutParams(locationBarLp);
                    }
                });
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CCT_ADAPTIVE_BUTTON_TEST_SWITCH, "always-animate", false)) {
            mOptionalButtonCoordinator.setAlwaysShowActionChip(true);
        }
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CCT_ADAPTIVE_BUTTON_TEST_SWITCH, "hide-button", false)) {
            // Simulate the toolbar that barely contains close, menu, 1 custom action button, and
            // the minimum location bar, which ends up with no space for the MTB.
            mModel.set(
                    TOOLBAR_WIDTH,
                    width * 3
                            + CustomTabToolbarButtonsViewBinder.getLocationBarMinWidth(
                                    mActivity.getResources(),
                                    mModel.get(OMNIBOX_ENABLED),
                                    mModel.get(TITLE_VISIBLE)));
        }
        return true;
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
