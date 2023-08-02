// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.android.httpclient.ChromeHttpClient;

/** Empty implementation of ReadAloudPlaybackHooks. */
public class ReadAloudPlaybackHooksImpl implements ReadAloudPlaybackHooks {
    public static ReadAloudPlaybackHooks getInstance(
            Context context, @Nullable String apiKeyOverride, ChromeHttpClient chromeHttpClient) {
        return new ReadAloudPlaybackHooks() {};
    }
}
