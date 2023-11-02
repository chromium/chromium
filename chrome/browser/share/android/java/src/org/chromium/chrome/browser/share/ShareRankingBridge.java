// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Arrays;
import java.util.List;

/**
 * This class is a shim that wraps the JNI interface to the C++-side
 * ShareHistory object.
 */
public class ShareRankingBridge {
    public static void rank(Profile profile, String type, List<String> available, int fold,
            int length, boolean persist, Callback<List<String>> onDone) {
        assert profile != null;
        ShareRankingBridgeJni.get().rank(profile, type, available.toArray(), fold, length, persist,
                result -> { onDone.onResult(Arrays.asList(result)); });
    }

    @NativeMethods
    public interface Natives {
        void rank(Profile profile, String type, Object[] available, int fold, int length,
                boolean persist, Callback<String[]> onDone);
    }
}
