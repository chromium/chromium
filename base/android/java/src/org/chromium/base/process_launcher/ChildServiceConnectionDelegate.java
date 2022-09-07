// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.os.IBinder;

/**
 * Delegate that ChildServiceConnection should call when the service connects/disconnects.
 * These callbacks are expected to happen on a background thread.
 */
/* package */ interface ChildServiceConnectionDelegate {
    void onServiceConnected(IBinder service);
    void onServiceDisconnected();
}
