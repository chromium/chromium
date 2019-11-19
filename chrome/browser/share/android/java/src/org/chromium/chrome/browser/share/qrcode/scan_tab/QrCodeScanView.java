// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

/**
 * Manages the Android View representing the QrCode scan panel.
 */
class QrCodeScanView {
    private final Context mContext;
    private final View mView;

    public QrCodeScanView(Context context) {
        mContext = context;

        mView = new FrameLayout(context);
        mView.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    public View getView() {
        return mView;
    }
}
