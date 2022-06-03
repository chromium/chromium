// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.Intent;

/** Noops Attribution Intent handling when the App Attribution feature isn't enabled. */
public class NoopAttributionIntentHandler implements AttributionIntentHandler {
    @Override
    public boolean handleOuterAttributionIntent(Intent intent) {
        return false;
    }

    @Override
    public Intent handleInnerAttributionIntent(Intent intent) {
        return null;
    }

    @Override
    public AttributionParameters getAndClearPendingAttributionParameters(Intent intent) {
        return null;
    }
}
