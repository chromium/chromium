// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

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

    public static void clear(Profile profile) {
        assert profile != null;
        ShareHistoryBridgeJni.get().clear(profile);
    }

    @NativeMethods
    public interface Natives {
        void addShareEntry(@JniType("Profile*") Profile profile, String string);

        void clear(@JniType("Profile*") Profile profile);
    }
}
