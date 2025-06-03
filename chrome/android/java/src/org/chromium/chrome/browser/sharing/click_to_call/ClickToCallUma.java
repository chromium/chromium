// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

/** Helper Class for Click to Call UMA Collection. */
@NullMarked
public class ClickToCallUma {
    public static void recordDialerPresent(boolean isDialerPresent) {
        RecordHistogram.recordBooleanHistogram("Sharing.ClickToCallDialerPresent", isDialerPresent);
    }
}
