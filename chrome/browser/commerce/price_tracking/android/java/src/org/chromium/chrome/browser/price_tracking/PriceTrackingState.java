// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.price_tracking;

/** The state of price tracking for the current tab. */
public enum PriceTrackingState {
    // Price tracking state is unknown for the current tab.
    UNKNOWN,
    // Price tracking is not eligible for the current tab.
    NOT_ELIGIBLE,
    // Price is untracked for the product in the current tab.
    UNTRACKED,
    // Price is tracked for the product in the current tab.
    TRACKED;
}
