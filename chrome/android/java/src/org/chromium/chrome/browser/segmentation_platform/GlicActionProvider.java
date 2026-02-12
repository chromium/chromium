// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;

/**
 * Action provider for the Glic contextual page action. This class determines if the Glic button
 * should be a candidate for the toolbar.
 */
@NullMarked
public class GlicActionProvider implements ContextualPageActionController.ActionProvider {

    public GlicActionProvider() {}

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        if (tab == null) {
            signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.GLIC, false);
            return;
        }
        boolean meetsCueingCriteria = checkCueingCriteria(tab);

        signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.GLIC, meetsCueingCriteria);
    }

    private boolean checkCueingCriteria(Tab tab) {
        // TODO(crbug.com/482372269): Evaluate whether the current tab meets the requirements to
        // show the Glic button by building a JNI bridge to contextual_cueing_service.h to get tab
        // information.
        if (tab == null) return false;
        return true;
    }
}
