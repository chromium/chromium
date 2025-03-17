// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.SHARE;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
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
    private Context mContext;
    private ActivityTabProvider mActivityTabProvider;
    private Drawable mOpenInBrowserButton;
    private Runnable mOpenInBrowserRunnable;
    private Runnable mRegisterVoiceSearchRunnable;
    private List<CustomButtonParams> mToolbarCustomButtons;
    private final Set<Integer> mValidButtons;

    public CustomTabAdaptiveToolbarBehavior(
            Context context,
            ActivityTabProvider activityTabProvider,
            List<CustomButtonParams> toolbarCustomButtons,
            Drawable openInBrowserButton,
            Runnable openInBrowserRunnable,
            Runnable registerVoiceSearchRunnable) {
        mContext = context;
        mActivityTabProvider = activityTabProvider;
        mToolbarCustomButtons = toolbarCustomButtons;
        mOpenInBrowserButton = openInBrowserButton;
        mOpenInBrowserRunnable = openInBrowserRunnable;
        mRegisterVoiceSearchRunnable = registerVoiceSearchRunnable;
        mValidButtons = new HashSet(COMMON_BUTTONS);
        if (ChromeFeatureList.sCctAdaptiveButtonEnableOpenInBrowser.getValue()) {
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
    public void registerPerSurfaceButtons(
            AdaptiveToolbarButtonController controller, Supplier<Tracker> trackerSupplier) {
        if (ChromeFeatureList.sCctAdaptiveButtonEnableVoice.getValue()) {
            mRegisterVoiceSearchRunnable.run();
        }

        if (ChromeFeatureList.sCctAdaptiveButtonEnableOpenInBrowser.getValue()) {
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

    @Override
    public int resultFilter(List<Integer> segmentationResults) {
        // If a customized button is specified by dev (or the default 'share' is on), find the first
        // result from |segmentationResults| that is not present in the customized ones.
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

        for (int result : segmentationResults) {
            if (!mValidButtons.contains(result)) continue;
            if ((OPEN_IN_BROWSER == result && hasCustomOpenInBrowser)
                    || (SHARE == result && hasCustomShare)) {
                continue;
            }
            return result;
        }
        return AdaptiveToolbarButtonVariant.UNKNOWN;
    }

    @Override
    public boolean useRawResults() {
        return true;
    }
}
