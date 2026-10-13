// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.insets.WindowInsetsUtils;

/**
 * Consumes system bar and display cutout insets while an installed web app draws edge-to-edge, so
 * the edge-to-edge root layout below draws zero-height system bars instead of padding the content.
 * Display cutout insets are only withheld while the window's cutout mode allows drawing into the
 * cutout, e.g. a TWA requesting {@code LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER} keeps them.
 *
 * <p>Safe area computations are unaffected: they read the raw window insets via {@link
 * InsetObserver#getLastRawWindowInsets()}. IME insets are never consumed, so keyboard-driven layout
 * keeps working while edge-to-edge.
 */
@NullMarked
public class WebappInsetsConsumer implements WindowInsetsConsumer {
    private final InsetObserver mInsetObserver;
    private boolean mDrawEdgeToEdge;

    /**
     * Creates the consumer and registers it on the given {@link InsetObserver} at {@link
     * InsetConsumerSource#WEB_APP_EDGE_TO_EDGE}.
     */
    public WebappInsetsConsumer(InsetObserver insetObserver) {
        mInsetObserver = insetObserver;
        mInsetObserver.addInsetsConsumer(this, InsetConsumerSource.WEB_APP_EDGE_TO_EDGE);
    }

    /**
     * Sets whether system bar and display cutout insets are withheld from downstream consumers
     * while the web app draws edge-to-edge. Re-dispatches the last seen window insets when the
     * state changes.
     */
    public void drawEdgeToEdge(boolean drawEdgeToEdge) {
        if (mDrawEdgeToEdge == drawEdgeToEdge) return;

        mDrawEdgeToEdge = drawEdgeToEdge;
        mInsetObserver.retriggerOnApplyWindowInsets();
    }

    @Override
    public WindowInsetsCompat onApplyWindowInsets(View view, WindowInsetsCompat insets) {
        if (!mDrawEdgeToEdge) return insets;

        WindowInsetsCompat insetsWithoutSystemBars =
                new WindowInsetsCompat.Builder(insets)
                        .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.NONE)
                        // EdgeToEdgeLayoutCoordinator falls back to these when the navigation
                        // bar insets equal their ignoring-visibility counterpart (OEM bugs,
                        // immersive mode); they must not re-introduce bottom padding while
                        // system bars are withheld.
                        .setInsets(WindowInsetsCompat.Type.tappableElement(), Insets.NONE)
                        .setInsets(WindowInsetsCompat.Type.mandatorySystemGestures(), Insets.NONE)
                        .build();
        // The window's cutout mode decides whether the layout below pads the content away from
        // the cutout, e.g. a TWA whose client requested LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER
        // keeps its cutout insets. Evaluated on the stripped insets: the layout pads system
        // bars from the insets it receives, so a cutout contained in a still-visible status bar
        // must not be consumed, otherwise the layout jumps when the bars hide.
        if (WindowInsetsUtils.shouldPadDisplayCutout(insetsWithoutSystemBars, view.getContext())) {
            return insetsWithoutSystemBars;
        }
        return new WindowInsetsCompat.Builder(insetsWithoutSystemBars)
                .setInsets(WindowInsetsCompat.Type.displayCutout(), Insets.NONE)
                .build();
    }

    /** Unregisters the consumer from the {@link InsetObserver}. */
    public void destroy() {
        mInsetObserver.removeInsetsConsumer(this);
    }
}
