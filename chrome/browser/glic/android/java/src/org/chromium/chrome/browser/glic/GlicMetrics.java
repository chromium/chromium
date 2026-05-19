// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;

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
}
