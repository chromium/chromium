// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import java.util.Map;

/** HttpClient callback interface. */
public interface HttpResponseCallback {
    void accept(int status, byte[] body, Map<String, String> headers);
}
