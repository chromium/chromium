// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Provides price insights signal for showing contextual page action for a given tab. */
public class PriceInsightsActionProvider implements ContextualPageActionController.ActionProvider {

    public PriceInsightsActionProvider() {}

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        if (tab == null || tab.getUrl() == null || !UrlUtilities.isHttpOrHttps(tab.getUrl())) {
            signalAccumulator.setHasPriceInsights(false);
            signalAccumulator.notifySignalAvailable();
            return;
        }
    }
}
