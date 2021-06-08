// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.Intent;

/** Public interface for the AttributionIntentHandler from the attribution_reporting module. */
public interface AttributionIntentHandler {
    /**
     * If |intent| is an outer App to Web Attribution Intent, processes the Intent and returns true,
     * otherwise returns false.
     */
    boolean handleOuterAttributionIntent(Intent intent);

    /**
     * If the intent is an inner App to Web Attribution Intent, processes the Attribution data, and
     * returns the View intent provided by the original outer intent.
     */
    Intent handleInnerAttributionIntent(Intent intent);

    /**
     * If the intent contains a valid pendingAttributionToken, returns the
     * AttributionParameters associated with the token, otherwise returns null.
     *
     * Note that this function clears the stored paramters and so will only work once for an Intent.
     */
    AttributionParameters getAndClearPendingAttributionParameters(Intent intent);
}
