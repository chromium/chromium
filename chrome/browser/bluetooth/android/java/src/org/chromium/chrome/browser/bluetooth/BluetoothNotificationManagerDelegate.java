// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bluetooth;

import android.content.Intent;

/** Delegate for {@link BluetoothNotificationManager}. */
public interface BluetoothNotificationManagerDelegate {
    /**
     * Creates an Intent to bring an Activity for a particular Tab back to the
     * foreground.
     * @param tabId The id of the Tab to bring to the foreground.
     * @return Created Intent or null if this operation isn't possible.
     */
    Intent createTrustedBringTabToFrontIntent(int tabId);

    /** Stops the service. */
    void stopSelf();

    /**
     * Stop the service if the most recent time it was started was startId.
     * @param startId Id for the service start request
     */
    void stopSelf(int startId);
}
