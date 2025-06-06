// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;

@NullMarked
public class TabGroupingActionProvider implements ContextualPageActionController.ActionProvider {

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        // TODO(salg): Integrate with group suggestions service.
        signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.TAB_GROUPING, false);
    }
}
