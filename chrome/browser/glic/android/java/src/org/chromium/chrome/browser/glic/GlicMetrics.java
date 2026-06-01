// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;

/** Metrics utils for use in Glic. */
@NullMarked
public class GlicMetrics {
    /** Recorded when the user clicks the 'X' to close the peek view. */
    public static void recordClosePeekView() {
        RecordUserAction.record("Glic.Instance.Close.PeekView");
    }

    /** Recorded when the Peek view appears on the screen. */
    public static void recordShowPeekView() {
        RecordUserAction.record("Glic.Instance.Show.PeekView");
    }

    /** Recorded when the Bottom Sheet appears on the screen. */
    public static void recordShowBottomSheet() {
        RecordUserAction.record("Glic.Instance.Show.BottomSheet");
    }

    /**
     * Recorded when a Glic entry point is clicked.
     *
     * @param source The GlicInvocationSource that triggered the click.
     * @param isNtp Whether the active tab is a New Tab Page.
     */
    public static void recordEntryPointClick(@GlicInvocationSource int source, boolean isNtp) {
        String tabContext = isNtp ? "Ntp" : "Other";
        RecordHistogram.recordEnumeratedHistogram(
                "Glic.EntryPoint.Click." + tabContext, source, GlicInvocationSource.MAX_VALUE);
    }
}
