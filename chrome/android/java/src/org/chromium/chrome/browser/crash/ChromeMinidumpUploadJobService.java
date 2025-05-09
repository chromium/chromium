// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.base.SplitCompatMinidumpUploadJobService;
import org.chromium.chrome.browser.metrics.UmaUtils;

/** See {@link ChromeMinidumpUploadJobServiceImpl}. */
@NullMarked
public class ChromeMinidumpUploadJobService extends SplitCompatMinidumpUploadJobService {
    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.crash.ChromeMinidumpUploadJobServiceImpl";

    public ChromeMinidumpUploadJobService() {
        super(sImplClassName);
    }

    @Override
    protected void recordMinidumpUploadingTime(long taskDurationMs) {
        UmaUtils.recordMinidumpUploadingTime(taskDurationMs);
    }
}
