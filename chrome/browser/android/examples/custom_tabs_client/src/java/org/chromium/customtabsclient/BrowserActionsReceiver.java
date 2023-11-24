// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.widget.Toast;

/**
 * A {@link BroadcastReceiver} that handles the callback if default menu items are chosen from
 * Browser Actions.
 */
public class BrowserActionsReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        String toastMsg = "Chosen item Id: " + intent.getDataString();
        Toast.makeText(context, toastMsg, Toast.LENGTH_SHORT).show();
    }
}
