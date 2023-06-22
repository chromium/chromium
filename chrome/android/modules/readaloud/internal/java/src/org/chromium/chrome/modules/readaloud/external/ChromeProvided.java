// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud.external;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.android.httpclient.ChromeHttpClient;

/** Chrome pieces needed by external ReadAloud code for starting playback. */
public interface ChromeProvided {
    /** Chrome version string. */
    default String chromeVersion() {
        return "";
    }

    /** API key for ReadAloud service. */
    default String readAloudApiKey() {
        return "";
    }

    /** HTTP client for making network calls through Chrome network stack. */
    default @Nullable ChromeHttpClient chromeHttpClient() {
        return null;
    }

    /** Get the Context. */
    default @Nullable Context context() {
        return null;
    }
}
