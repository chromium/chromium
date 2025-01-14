// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import org.chromium.build.annotations.NullMarked;

import java.util.Map;

/** HttpClient callback interface. */
@NullMarked
public interface HttpResponseCallback {
    void accept(int status, byte[] body, Map<String, String> headers);
}
