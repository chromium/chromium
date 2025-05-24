// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import android.os.Handler;
import android.os.Looper;
import android.util.Pair;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.DistillationStatus;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.url.GURL;

import java.util.Objects;

/** Provides reader mode signal for showing contextual page action for a given tab. */
@NullMarked
public class ReaderModeActionProvider implements ContextualPageActionController.ActionProvider {
    /** Histogram name for if any distillation signal was in time for the CPA timeout. */
    public static final String SIGNAL_ACCUMULATOR_WITHIN_TIMEOUT_HISTOGRAM =
            "DomDistiller.Android.AnyPageSignalWithinTimeout";

    /** Histogram name for if a positive distillation signal was in time for the CPA timeout. */
    public static final String SIGNAL_ACCUMULATOR_DISTILLABLE_WITHIN_TIMEOUT_HISTOGRAM =
            "DomDistiller.Android.DistillablePageSignalWithinTimeout";

    // DistillabilityObserver which automatically un/registers itself as an observer when there is a
    // result.
    private class OneshotDistillabilityObserver extends EmptyTabObserver
            implements DistillabilityObserver {
        private final Tab mTab;
        private final TabDistillabilityProvider mDistillabilityProvider;
        private final SignalAccumulator mSignalAccumulator;

        private boolean mIsDestroyed;

        OneshotDistillabilityObserver(
                Tab tab,
                TabDistillabilityProvider distillabilityProvider,
                SignalAccumulator signalAccumulator) {
            mTab = tab;
            mDistillabilityProvider = distillabilityProvider;
            mSignalAccumulator = signalAccumulator;

            mTab.addObserver(this);
            // If distillability is already determined, then call the obs method directly. Otherwise
            // register the observer and wait.
            if (mDistillabilityProvider.isDistillabilityDetermined()) {
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT,
                        () ->
                                onIsPageDistillableResult(
                                        mTab,
                                        mDistillabilityProvider.isDistillable(),
                                        /* isLast= */ true,
                                        mDistillabilityProvider.isMobileOptimized()));
            } else {
                mDistillabilityProvider.addObserver(this);
            }
        }

        public void destroy() {
            if (mIsDestroyed) return;
            mIsDestroyed = true;
            mTab.removeObserver(this);
            // No-op when this isn't already added as an observer.
            mDistillabilityProvider.removeObserver(this);
        }

        // EmptyTabObserver implementation.

        @Override
        public void onHidden(Tab tab, @TabHidingType int type) {
            destroy();
        }

        @Override
        public void onDestroyed(Tab tab) {
            destroy();
        }

        // DistillabilityObserver implementation.

        @Override
        public void onIsPageDistillableResult(
                Tab tab, boolean isDistillable, boolean isLast, boolean isMobileOptimized) {
            if (!Objects.equals(tab.getUrl(), ReaderModeActionProvider.this.mLastSeenUrl)) {
                return;
            }

            Pair<Boolean, Integer> result =
                    ReaderModeManager.computeDistillationStatus(
                            tab, isDistillable, isMobileOptimized, /* isLast= */ true);
            if (result.first) {
                notifyActionAvailable(
                        result.second == DistillationStatus.POSSIBLE, mSignalAccumulator);
                destroy();
            }
        }
    }

    private @Nullable OneshotDistillabilityObserver mDistillabilityObserver;
    private @Nullable GURL mLastSeenUrl;

    // ContextualPageActionController.ActionProvider implementation.

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        if (mDistillabilityObserver != null) {
            mDistillabilityObserver.destroy();
        }

        if (tab == null) return;
        final TabDistillabilityProvider tabDistillabilityProvider =
                TabDistillabilityProvider.get(tab);
        if (tabDistillabilityProvider == null) return;

        // Distillability score isn't available yet. Start observing the provider.
        mLastSeenUrl = tab.getUrl();
        mDistillabilityObserver =
                new OneshotDistillabilityObserver(
                        tab, tabDistillabilityProvider, signalAccumulator);
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

    @Override
    public void destroy() {
        if (mDistillabilityObserver != null) {
            mDistillabilityObserver.destroy();
        }
    }

    private void notifyActionAvailable(boolean isDistillable, SignalAccumulator signalAccumulator) {
        // TODO(shaktisahu): Can we merge these into a single method call?
        signalAccumulator.setHasReaderMode(isDistillable);
        signalAccumulator.notifySignalAvailable();

        boolean signalAvailable = !signalAccumulator.hasTimedOut();
        RecordHistogram.recordBooleanHistogram(
                SIGNAL_ACCUMULATOR_WITHIN_TIMEOUT_HISTOGRAM, signalAvailable);
        // Record if the signal counted when a page was distillable.
        if (isDistillable) {
            RecordHistogram.recordBooleanHistogram(
                    SIGNAL_ACCUMULATOR_DISTILLABLE_WITHIN_TIMEOUT_HISTOGRAM, signalAvailable);
        }
    }
}
