// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.content.Context;

import org.chromium.chrome.browser.share.qrcode.scan_tab.QrCodeScanCoordinator;
import org.chromium.chrome.browser.share.qrcode.share_tab.QrCodeShareCoordinator;

/**
 * Creates and represents the QrCode main UI.
 */
public class QrCodeCoordinator {
    QrCodeDialog mDialog;

    QrCodeCoordinator(Context context) {
        QrCodeShareCoordinator shareCoordinator = new QrCodeShareCoordinator(context);
        QrCodeScanCoordinator scanCoordinator = new QrCodeScanCoordinator(context);

        mDialog = new QrCodeDialog(context, shareCoordinator.getView(), scanCoordinator.getView());
    }

    public void show() {
        mDialog.show();
    }
}
