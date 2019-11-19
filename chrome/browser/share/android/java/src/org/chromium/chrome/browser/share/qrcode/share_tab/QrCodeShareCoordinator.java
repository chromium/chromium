// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.content.Context;
import android.view.View;

/**
 * Creates and represents the QrCode share panel UI.
 */
public class QrCodeShareCoordinator {
    private final QrCodeShareView mShareView;

    public QrCodeShareCoordinator(Context context) {
        mShareView = new QrCodeShareView(context);
    }

    public View getView() {
        return mShareView.getView();
    }
}