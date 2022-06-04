// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.widget.Toast;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/**
 * Coordinator for displaying the send tab to self feature.
 */
public class SendTabToSelfCoordinator {

    private final Context mContext;

    public SendTabToSelfCoordinator(Context context) {
        mContext = context;
    }

    public void show() {
        Toast.makeText(mContext, "SendTabToSelfCoordinator", Toast.LENGTH_SHORT).show();
    }

}
