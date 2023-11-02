// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.os.Bundle;

/**
 * Cancel the startActivity animation used to keep 2D UI hidden while Chrome is starting up.
 * See the comments in VrShellDelegate.VrBroadcastReceiver#onReceive for more information.
 */
public class VrCancelAnimationActivity extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        finish();
        overridePendingTransition(0, 0);
    }
}