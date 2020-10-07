// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;

/**
 * Runs the {@link OmahaBase} pipeline as a {@link IntentService}.
 *
 * NOTE: This class can never be renamed because the user may have Intents floating around that
 *       reference this class specifically.
 */
public class OmahaClientImpl extends OmahaClient.Impl {
    @Override
    protected void onServiceSet() {
        getService().setIntentRedelivery(true);
    }

    @Override
    public void onHandleIntent(Intent intent) {
        OmahaService.getInstance(getService()).run();
    }

    static Intent createIntent(Context context) {
        return new Intent(context, OmahaClient.class);
    }
}
