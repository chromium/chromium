// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import java.util.Map;

/** A simple HttpClient interface. */
public interface ChromeHttpClient {
    void send(
            String url,
            String requestType,
            byte[] body,
            Map<String, String> headers,
            HttpResponseCallback responseCallback);
}
