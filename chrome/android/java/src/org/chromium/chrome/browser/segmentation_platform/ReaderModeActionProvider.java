// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.chrome.browser.tab.Tab;

/** Provides reader mode signal for showing contextual page action for a given tab. */
public class ReaderModeActionProvider implements ContextualPageActionController.ActionProvider {
    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        final TabDistillabilityProvider tabDistillabilityProvider =
                TabDistillabilityProvider.get(tab);
        if (tabDistillabilityProvider.isDistillabilityDetermined()) {
            notifyActionAvailable(tabDistillabilityProvider.isDistillable(),
                    tabDistillabilityProvider.isMobileOptimized(), tab, signalAccumulator);
            return;
        }

        // Distillability score isn't available yet. Start observing the provider.
        final TabDistillabilityProvider.DistillabilityObserver distillabilityObserver =
                new DistillabilityObserver() {
                    @Override
                    public void onIsPageDistillableResult(Tab tab, boolean isDistillable,
                            boolean isLast, boolean isMobileOptimized) {
                        notifyActionAvailable(
                                isDistillable, isMobileOptimized, tab, signalAccumulator);
                        tabDistillabilityProvider.removeObserver(this);
                    }
                };
        tabDistillabilityProvider.addObserver(distillabilityObserver);
    }

    private void notifyActionAvailable(boolean isDistillable, boolean isMobileOptimized, Tab tab,
            SignalAccumulator signalAccumulator) {
        // TODO(shaktisahu): Can we merge these into a single method call?
        signalAccumulator.setHasReaderMode(isDistillable && !isFilteredOut(tab, isMobileOptimized));
        signalAccumulator.notifySignalAvailable();
    }

    private boolean isFilteredOut(Tab tab, boolean isMobileOptimized) {
        // Test if the user is requesting the desktop site. Ignore this if distiller is set to
        // ALWAYS_TRUE.
        boolean usingRequestDesktopSite = tab.getWebContents() != null
                && tab.getWebContents().getNavigationController().getUseDesktopUserAgent()
                && !DomDistillerTabUtils.isHeuristicAlwaysTrue();

        if (usingRequestDesktopSite) return true;

        if (isMobileOptimized && DomDistillerTabUtils.shouldExcludeMobileFriendly(tab)) {
            return true;
        }

        // TODO(crbug/1373891): Add rate limiting logic for muted sites.
        return false;
    }
}
