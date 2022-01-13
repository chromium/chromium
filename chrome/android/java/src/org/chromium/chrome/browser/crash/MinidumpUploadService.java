// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatIntentService;

/** See {@link MinidumpUploadServiceImpl}. */
public class MinidumpUploadService extends SplitCompatIntentService {
    private static final String TAG = "MinidmpUploadService";

    @IdentifierNameString
    private static final String IMPL_CLASS_NAME =
            "org.chromium.chrome.browser.crash.MinidumpUploadServiceImpl";

    public MinidumpUploadService() {
        super(IMPL_CLASS_NAME, TAG);
    }
}
