// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import android.os.Handler;
import android.os.Looper;

import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;

/** Provides reader mode signal for showing contextual page action for a given tab. */
public class ReaderModeActionProvider implements ContextualPageActionController.ActionProvider {
    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        final TabDistillabilityProvider tabDistillabilityProvider =
                TabDistillabilityProvider.get(tab);
        if (tabDistillabilityProvider.isDistillabilityDetermined()) {
            notifyActionAvailable(
                    tabDistillabilityProvider.isDistillable(),
                    tabDistillabilityProvider.isMobileOptimized(),
                    tab,
                    signalAccumulator);
            return;
        }

        // Distillability score isn't available yet. Start observing the provider.
        final TabDistillabilityProvider.DistillabilityObserver distillabilityObserver =
                new DistillabilityObserver() {
                    @Override
                    public void onIsPageDistillableResult(
                            Tab tab,
                            boolean isDistillable,
                            boolean isLast,
                            boolean isMobileOptimized) {
                        notifyActionAvailable(
                                isDistillable, isMobileOptimized, tab, signalAccumulator);
                        tabDistillabilityProvider.removeObserver(this);
                    }
                };
        tabDistillabilityProvider.addObserver(distillabilityObserver);
    }

    @Override
    public void onActionShown(Tab tab, @AdaptiveToolbarButtonVariant int action) {
        if (tab == null) return;
        if (action != AdaptiveToolbarButtonVariant.READER_MODE) return;

        new Handler(Looper.getMainLooper())
                .postDelayed(
                        () -> {
                            if (tab.isDestroyed()) return;

                            ReaderModeManager readerModeManager =
                                    tab.getUserDataHost()
                                            .getUserData(ReaderModeManager.USER_DATA_KEY);
                            if (readerModeManager != null) {
                                readerModeManager.setReaderModeUiShown();
                            }
                        },
                        /* delayMillis= */ 500);
    }

    private void notifyActionAvailable(
            boolean isDistillable,
            boolean isMobileOptimized,
            Tab tab,
            SignalAccumulator signalAccumulator) {
        // TODO(shaktisahu): Can we merge these into a single method call?
        signalAccumulator.setHasReaderMode(isDistillable && !isFilteredOut(tab, isMobileOptimized));
        signalAccumulator.notifySignalAvailable();
    }

    private boolean isFilteredOut(Tab tab, boolean isMobileOptimized) {
        // Test if the user is requesting the desktop site. Ignore this if distiller is set to
        // ALWAYS_TRUE.
        boolean usingRequestDesktopSite =
                tab.getWebContents() != null
                        && tab.getWebContents().getNavigationController().getUseDesktopUserAgent()
                        && !DomDistillerTabUtils.isHeuristicAlwaysTrue();

        if (usingRequestDesktopSite) return true;

        if (isMobileOptimized && DomDistillerTabUtils.shouldExcludeMobileFriendly(tab)) {
            return true;
        }

        return false;
    }
}
