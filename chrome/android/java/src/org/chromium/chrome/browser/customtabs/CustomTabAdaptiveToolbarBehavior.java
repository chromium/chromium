// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.feature_engagement.Tracker;

import java.util.List;

/** Implements CustomTab-specific behavior of adaptive toolbar button. */
public class CustomTabAdaptiveToolbarBehavior implements AdaptiveToolbarBehavior {

    private Runnable mRegisterVoiceSearchRunnable;

    public CustomTabAdaptiveToolbarBehavior(Runnable registerVoiceSearchRunnable) {
        mRegisterVoiceSearchRunnable = registerVoiceSearchRunnable;
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
            // TODO(crbug.com/391931152): Instantiate ButtonDataProvider for the action
            //    'open in browser'.
        }
    }

    @Override
    public int resultFilter(List<Integer> segmentationResults) {
        // TODO(crbug.com/391931152): If a customized button is specified by dev (or the default
        //    'share' is on), find the first result from |segmentationResults| that is not
        //    duplicated in the customized ones.
        return AdaptiveToolbarButtonVariant.UNKNOWN;
    }

    @Override
    public boolean useRawResults() {
        return true;
    }
}
