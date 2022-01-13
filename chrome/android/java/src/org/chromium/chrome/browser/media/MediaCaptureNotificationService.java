// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatService;

/** See {@link MediaCaptureNotificationServiceImpl}. */
public class MediaCaptureNotificationService extends SplitCompatService {
    @IdentifierNameString
    private static final String IMPL_CLASS_NAME =
            "org.chromium.chrome.browser.media.MediaCaptureNotificationServiceImpl";

    public MediaCaptureNotificationService() {
        super(IMPL_CLASS_NAME);
    }
}
