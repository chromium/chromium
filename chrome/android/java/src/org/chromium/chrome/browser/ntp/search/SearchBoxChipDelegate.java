// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.graphics.Bitmap;

import org.chromium.base.Callback;

/**
 * This class is responsible for reacting to events from the chip on the search box.
 */
public interface SearchBoxChipDelegate {
    /** Called when the chip is clicked. */
    void onChipClicked();

    /** Called when the cancel button on the chip is clicked. */
    void onCancelClicked();

    /** Called to retrieve the bitmap to be displayed on the chip icon. */
    void getChipIcon(Callback<Bitmap> callback);
}
