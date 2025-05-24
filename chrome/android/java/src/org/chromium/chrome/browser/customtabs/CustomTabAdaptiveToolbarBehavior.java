// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.OPEN_IN_BROWSER_STATE_DEFAULT;

import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.SHARE;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.browser.customtabs.ExperimentalOpenInBrowser;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.OpenInBrowserButtonController;
import org.chromium.components.feature_engagement.Tracker;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Implements CustomTab-specific behavior of adaptive toolbar button. */
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

    @ExperimentalOpenInBrowser
    @Override
    public void registerPerSurfaceButtons(
            AdaptiveToolbarButtonController controller, Supplier<Tracker> trackerSupplier) {
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

    @Override
    public int resultFilter(List<Integer> segmentationResults) {
        // If a customized button is specified by dev (or the default 'share' is on), find the first
        // result from |segmentationResults| that is not present in the customized ones.
        // Try the next best one if the top one is not available.
        for (int i = 0; i < Math.min(segmentationResults.size(), 2); ++i) {
            int result = segmentationResults.get(i);
            if (mValidButtons.contains(result) && !isButtonDuplicated(result)) return result;
        }
        return AdaptiveToolbarButtonVariant.UNKNOWN;
    }

    @Override
    public boolean canShowManualOverride(@AdaptiveToolbarButtonVariant int manualOverride) {
        // Manual override should not be shown if the developer specified the same type
        // in the custom action buttons.
        return !isButtonDuplicated(manualOverride);
    }

    @Override
    public boolean useRawResults() {
        return true;
    }

    @Override
    public @AdaptiveToolbarButtonVariant int getSegmentationDefault() {
        // CCT MTB doesn't provide a default action. The button will be hidden if there is
        // no action to display.
        return AdaptiveToolbarButtonVariant.UNKNOWN;
    }

    @ExperimentalOpenInBrowser
    private boolean isOpenInBrowserButtonEnabled() {
        return ChromeFeatureList.sCctAdaptiveButtonEnableOpenInBrowser.getValue()
                && mIntentDataProvider.getOpenInBrowserButtonState()
                        == OPEN_IN_BROWSER_STATE_DEFAULT;
    }
}
