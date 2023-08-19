// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatIntentService;

/** See {@link MinidumpUploadServiceImpl}. */
public class MinidumpUploadService extends SplitCompatIntentService {
    private static final String TAG = "MinidmpUploadService";

    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.crash.MinidumpUploadServiceImpl";

    public MinidumpUploadService() {
        super(sImplClassName, TAG);
    }
}
