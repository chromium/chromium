// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import org.chromium.chrome.browser.base.SplitCompatIntentService;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** See {@link OmahaClientImpl}. */
public class OmahaClient extends SplitCompatIntentService {
    private static final String TAG = "omaha";

    public OmahaClient() {
        super(SplitCompatUtils.getIdentifierName(
                      "org.chromium.chrome.browser.omaha.OmahaClientImpl"),
                TAG);
    }
}
