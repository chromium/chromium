// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

/**
 * Manages the Android View representing the QrCode share panel.
 */
class QrCodeShareView {
    private final Context mContext;
    private final View mView;

    public QrCodeShareView(Context context) {
        mContext = context;

        mView = (View) LayoutInflater.from(context).inflate(
                org.chromium.chrome.browser.share.qrcode.R.layout.qrcode_share_layout, null, false);
    }

    public View getView() {
        return mView;
    }
}
