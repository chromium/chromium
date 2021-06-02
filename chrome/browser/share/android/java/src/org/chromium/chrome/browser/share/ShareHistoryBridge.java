// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * This class is a shim that wraps the JNI interface to the C++-side
 * ShareHistory object.
 */
public class ShareHistoryBridge {
    public static void addShareEntry(Profile profile, String target) {
        assert profile != null;
        ShareHistoryBridgeJni.get().addShareEntry(profile, target);
    }

    @NativeMethods
    public interface Natives {
        void addShareEntry(Profile profile, String string);
    }
}
