// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import android.os.Handler;
import android.os.Looper;
import android.util.Pair;

import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.DistillationStatus;
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
        // TODO(crbug.com/405420546): Consider emitting a signal from TabDistillabilityProvider when
        // an observer is added if distillability is already determined.
        if (tabDistillabilityProvider.isDistillabilityDetermined()) {
            Pair<Boolean, Integer> result =
                    ReaderModeManager.computeDistillationStatus(
                            tab,
                            tabDistillabilityProvider.isDistillable(),
                            tabDistillabilityProvider.isMobileOptimized(),
                            /* isLast= */ true);
            notifyActionAvailable(result.second == DistillationStatus.POSSIBLE, signalAccumulator);
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
                        Pair<Boolean, Integer> result =
                                ReaderModeManager.computeDistillationStatus(
                                        tab,
                                        tabDistillabilityProvider.isDistillable(),
                                        tabDistillabilityProvider.isMobileOptimized(),
                                        /* isLast= */ true);
                        if (result.first) {
                            notifyActionAvailable(
                                    result.second == DistillationStatus.POSSIBLE,
                                    signalAccumulator);
                            tabDistillabilityProvider.removeObserver(this);
                        }
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

    private void notifyActionAvailable(boolean isDistillable, SignalAccumulator signalAccumulator) {
        // TODO(shaktisahu): Can we merge these into a single method call?
        signalAccumulator.setHasReaderMode(isDistillable);
        signalAccumulator.notifySignalAvailable();
    }
}
