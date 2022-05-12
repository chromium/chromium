// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Contains JNI methods to needed by read later feature.
 */
public final class ReadingListBridge {

    @CalledByNative
    private static String getNotificationTitle() {
        return null;
    }

    @CalledByNative
    private static String getNotificationText(int unreadSize) {
        return null;
    }

    @CalledByNative
    private static void openReadingListPage() {
    }

    @NativeMethods
    interface Natives {
        void onStartChromeForeground();
    }
}
