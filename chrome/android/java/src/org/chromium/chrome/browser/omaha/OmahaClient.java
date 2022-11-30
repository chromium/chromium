// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatIntentService;

/** See {@link OmahaClientImpl}. */
public class OmahaClient extends SplitCompatIntentService {
    private static final String TAG = "omaha";

    @IdentifierNameString
    private static String sImplClassName = "org.chromium.chrome.browser.omaha.OmahaClientImpl";

    public OmahaClient() {
        super(sImplClassName, TAG);
    }
}
