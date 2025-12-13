// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.OPEN_IN_BROWSER_STATE_OFF;
import static androidx.browser.customtabs.CustomTabsIntent.SHARE_STATE_OFF;

import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabMtbHiddenReason.COUNT;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabMtbHiddenReason.CPA_ONLY_MODE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabMtbHiddenReason.DUPLICATED_ACTION;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabMtbHiddenReason.INVALID_VARIANT;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabMtbHiddenReason.OTHER_REASON;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.SHARE;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.UNKNOWN;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.browser.customtabs.ExperimentalOpenInBrowser;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabMtbHiddenReason;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.OpenInBrowserButtonController;
import org.chromium.components.feature_engagement.Tracker;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/** Implements CustomTab-specific behavior of adaptive toolbar button. */
@NullMarked
public class CustomTabAdaptiveToolbarBehavior implements AdaptiveToolbarBehavior {
    private final Context mContext;
    private final ActivityTabProvider mActivityTabProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final Drawable mOpenInBrowserButton;
    private final Runnable mOpenInBrowserRunnable;
    private final Runnable mRegisterVoiceSearchRunnable;
    private final List<CustomButtonParams> mToolbarCustomButtons;
    private final Set<Integer> mValidButtons;

    @ExperimentalOpenInBrowser
    public CustomTabAdaptiveToolbarBehavior(
            Context context,
            ActivityTabProvider activityTabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            Drawable openInBrowserButton,
            Runnable openInBrowserRunnable,
            Runnable registerVoiceSearchRunnable) {
        mContext = context;
        mActivityTabProvider = activityTabProvider;
        mIntentDataProvider = intentDataProvider;
        mToolbarCustomButtons = mIntentDataProvider.getCustomButtonsOnToolbar();
        mOpenInBrowserButton = openInBrowserButton;
        mOpenInBrowserRunnable = openInBrowserRunnable;
        mRegisterVoiceSearchRunnable = registerVoiceSearchRunnable;
        mValidButtons = new HashSet(COMMON_BUTTONS);
        if (isOpenInBrowserButtonEnabled()) {
            mValidButtons.add(AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER);
        }
        if (!isShareButtonEnabled()) mValidButtons.remove(SHARE);
        if (ChromeFeatureList.sCctAdaptiveButtonEnableVoice.getValue()) {
            mValidButtons.add(AdaptiveToolbarButtonVariant.VOICE);
        }
    }

    @Override
    public boolean shouldInitialize() {
        return ChromeFeatureList.sCctAdaptiveButton.isEnabled();
    }

    @Override
    public boolean canShowSettings() {
        return false;
    }

    @Override
    public boolean shouldShowTextBubble() {
        int screenWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
        return screenWidthDp < AdaptiveToolbarFeatures.MAX_WIDTH_FOR_BUBBLE_DP;
    }

    @ExperimentalOpenInBrowser
    @Override
    public void registerPerSurfaceButtons(
            AdaptiveToolbarButtonController controller,
            Supplier<@Nullable Tracker> trackerSupplier) {
        if (ChromeFeatureList.sCctAdaptiveButtonEnableVoice.getValue()) {
            mRegisterVoiceSearchRunnable.run();
        }

        if (isOpenInBrowserButtonEnabled()) {
            var openInBrowserButton =
                    new OpenInBrowserButtonController(
                            mContext,
                            mOpenInBrowserButton,
                            mActivityTabProvider,
                            mOpenInBrowserRunnable,
                            trackerSupplier);
            controller.addButtonVariant(
                    AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER, openInBrowserButton);
        }
    }

    private boolean isButtonDuplicated(@AdaptiveToolbarButtonVariant int button) {
        boolean hasCustomOpenInBrowser = false;
        boolean hasCustomShare = false;
        for (CustomButtonParams params : mToolbarCustomButtons) {
            switch (params.getType()) {
                case ButtonType.CCT_OPEN_IN_BROWSER_BUTTON:
                    hasCustomOpenInBrowser = true;
                    break;
                case ButtonType.CCT_SHARE_BUTTON:
                    hasCustomShare = true;
                    break;
            }
        }
        return (OPEN_IN_BROWSER == button && hasCustomOpenInBrowser)
                || (SHARE == button && hasCustomShare);
    }

    @ExperimentalOpenInBrowser
    @Override
    public int resultFilter(List<Integer> segmentationResults) {
        // If a customized button is specified by dev (or the default 'share' is on), find the first
        // result from |segmentationResults| that is not present in the customized ones.
        // Try the next best one if the top one is not available.
        @CustomTabMtbHiddenReason int hiddenReason = OTHER_REASON;
        for (int i = 0; i < Math.min(segmentationResults.size(), 2); ++i) {
            int result = segmentationResults.get(i);
            boolean isValid = true;
            if (!mValidButtons.contains(result)) {
                if (hiddenReason == OTHER_REASON) hiddenReason = INVALID_VARIANT;
                isValid = false;
            } else if (isButtonDuplicated(result)) {
                if (hiddenReason == OTHER_REASON) hiddenReason = DUPLICATED_ACTION;
                isValid = false;
            } else if (shouldSkipStaticAction(result)) {
                if (hiddenReason == OTHER_REASON) hiddenReason = CPA_ONLY_MODE;
                isValid = false;
            }
            if (isValid) {
                RecordHistogram.recordEnumeratedHistogram(
                        "CustomTabs.AdaptiveToolbarButton.ChosenRanking", i, 2);
                return result;
            }
        }

        // If both 2 buttons are invalid, log the reason for the first variant.
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.AdaptiveToolbarButton.HiddenReason", hiddenReason, COUNT);
        return AdaptiveToolbarButtonVariant.UNKNOWN;
    }

    /** Whether some static action should be filtered out. */
    @ExperimentalOpenInBrowser
    private boolean shouldSkipStaticAction(@AdaptiveToolbarButtonVariant int variant) {
        if (!AdaptiveToolbarFeatures.isDynamicAction(variant)) {
            // |contextual_only| filters out all the static actions, unless 'open in browser'
            // is explicitly enabled and developers wish to use it.
            if (ChromeFeatureList.sCctAdaptiveButtonContextualOnly.getValue()) {
                return !(isOpenInBrowserButtonEnabled() && variant == OPEN_IN_BROWSER);
            }
        }
        return false;
    }

    @Override
    public boolean canShowManualOverride(@AdaptiveToolbarButtonVariant int manualOverride) {
        // Manual override should not be shown if the developer specified the same type
        // in the custom action buttons or Chrome Actions is set to off. Also, for the
        // configuration CPA+OpenInBrowserDefault, the default should show over the manual
        // override.
        if (isButtonDuplicated(manualOverride)) return false;
        if (manualOverride == SHARE && !isShareButtonEnabled()) return false;
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CCT_ADAPTIVE_BUTTON, "contextual_only", false)
                && ChromeFeatureList.sCctAdaptiveButtonDefaultVariant.getValue()
                        == OPEN_IN_BROWSER) {
            return false;
        }
        return true;
    }

    @Override
    public boolean useRawResults() {
        return true;
    }

    @Override
    public @AdaptiveToolbarButtonVariant int getSegmentationDefault() {
        var defVariant = ChromeFeatureList.sCctAdaptiveButtonDefaultVariant.getValue();
        return isButtonDuplicated(defVariant) ? UNKNOWN : defVariant;
    }

    @ExperimentalOpenInBrowser
    private boolean isOpenInBrowserButtonEnabled() {
        return mIntentDataProvider.getOpenInBrowserButtonState() != OPEN_IN_BROWSER_STATE_OFF;
    }

    private boolean isShareButtonEnabled() {
        return mIntentDataProvider.getShareButtonState() != SHARE_STATE_OFF;
    }
}
