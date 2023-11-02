// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.app.IntentService;
import android.content.Intent;

/**
 * In the past, this class sent upstream messages for Invalidations using GCM.
 * It's now empty, but can't be deleted yet because it is listed in
 * AndroidManifest.xml.
 */
public class InvalidationGcmUpstreamSender extends IntentService {
    InvalidationGcmUpstreamSender() {
        super("GcmUpstreamService");
    }

    @Override
    protected void onHandleIntent(Intent intent) {}
}
