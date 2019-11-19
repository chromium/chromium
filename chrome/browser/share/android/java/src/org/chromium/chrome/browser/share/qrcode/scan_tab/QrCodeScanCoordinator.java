// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.content.Context;
import android.view.View;

/**
 * Creates and represents the QrCode scan panel UI.
 */
public class QrCodeScanCoordinator {
    private final QrCodeScanView mScanView;

    public QrCodeScanCoordinator(Context context) {
        mScanView = new QrCodeScanView(context);
    }

    public View getView() {
        return mScanView.getView();
    }
}