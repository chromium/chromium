// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareActivity;

/**
 * A simple activity that shows sharing QR code option in share menu.
 */
public class QrCodeShareActivity extends ShareActivity {
    @Override
    protected void handleShareAction(ChromeActivity triggeringActivity) {
        QrCodeCoordinator qrCodeCoordinator = new QrCodeCoordinator(triggeringActivity);
        qrCodeCoordinator.show();
    }

    public static boolean featureIsAvailable() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SHARING_QR_CODE_ANDROID);
    }
}