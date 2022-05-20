// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.widget.Toast;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.sync.ModelType;
import org.chromium.ui.base.WindowAndroid;

/**
 * Coordinator for displaying the send tab to self feature.
 */
public class SendTabToSelfCoordinator {
    /** Waits for Sync to download the list of target devices after sign-in. */
    private static class TargetDeviceListWaiter implements SyncService.SyncStateChangedListener {
        private final Promise<Void> mPromise = new Promise<Void>();

        public TargetDeviceListWaiter() {
            SyncService.get().addSyncStateChangedListener(this);
            fullfillIfReady();
        }

        public Promise<Void> waitUntilReady() {
            return mPromise;
        }

        @Override
        public void syncStateChanged() {
            fullfillIfReady();
        }

        private void fullfillIfReady() {
            if (SyncService.get().getActiveDataTypes().contains(ModelType.DEVICE_INFO)) {
                SyncService.get().removeSyncStateChangedListener(this);
                mPromise.fulfill(null);
            }
        }
    }

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final String mUrl;
    private final String mTitle;
    private final BottomSheetController mController;

    public SendTabToSelfCoordinator(Context context, WindowAndroid windowAndroid, String url,
            String title, BottomSheetController controller) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mUrl = url;
        mTitle = title;
        mController = controller;
    }

    public void show() {
        Toast.makeText(mContext, "SendTabToSelfCoordinator", Toast.LENGTH_SHORT).show();
    }

    private void showDeviceList() {
    }

    private boolean shouldOfferSignInPromo() {
        return false;
    }
}
