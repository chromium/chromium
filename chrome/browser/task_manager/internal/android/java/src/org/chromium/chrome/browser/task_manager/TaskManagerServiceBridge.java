// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import org.jni_zero.NativeMethods;

class TaskManagerServiceBridge {

    @NativeMethods
    interface Natives {
        int five();
    }

    int getFive() {
        Natives jni = TaskManagerServiceBridgeJni.get();
        return jni.five();
    }
}
