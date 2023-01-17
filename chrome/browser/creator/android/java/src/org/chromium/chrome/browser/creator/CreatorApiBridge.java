// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * A Java API for connecting to creator component
 */
@JNINamespace("creator")
public class CreatorApiBridge {
    /**
     * A Java Class to store creator metadata.
     */
    public static class Creator {
        public final String url;
        public final String title;

        @CalledByNative("Creator")
        public Creator(String url, String title) {
            this.url = url;
            this.title = title;
        }
    }

    public static void getCreator(String webChannelId, Callback<Creator> callback) {
        CreatorApiBridgeJni.get().getCreator(webChannelId, callback);
    }

    public static void getWebId(String url, Callback<String> callback) {
        CreatorApiBridgeJni.get().getWebId(url, callback);
    }

    @NativeMethods
    interface Natives {
        void getCreator(String webChannelId, Callback<Creator> callback);
        void getWebId(String url, Callback<String> callback);
    }
}
