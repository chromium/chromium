// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.chrome.browser.base.SplitCompatService;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** See {@link MediaCaptureNotificationServiceImpl}. */
public class MediaCaptureNotificationService extends SplitCompatService {
    public MediaCaptureNotificationService() {
        super(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.media.MediaCaptureNotificationServiceImpl"));
    }
}
