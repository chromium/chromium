// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatIntentService;

/** See {@link OmahaClientImpl}. */
public class OmahaClient extends SplitCompatIntentService {
    private static final String TAG = "omaha";

    @IdentifierNameString
    private static final String IMPL_CLASS_NAME =
            "org.chromium.chrome.browser.omaha.OmahaClientImpl";

    public OmahaClient() {
        super(IMPL_CLASS_NAME, TAG);
    }
}
