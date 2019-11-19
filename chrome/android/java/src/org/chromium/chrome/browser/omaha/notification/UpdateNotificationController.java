// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.notification;

import android.content.Intent;

/**
 * Helper for gluing different implementations of UpdateNotificationController.
 */
public interface UpdateNotificationController {
    /**
     * Receives and handles intent.
     * @param intent A {@link Intent} could contain an extra that helpful for controller.
     */
    void onNewIntent(Intent intent);
}
