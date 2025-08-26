// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import static org.chromium.build.NullUtil.assertNonNull;

import android.os.PersistableBundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.base.SplitCompatMinidumpUploadJobService;
import org.chromium.components.minidump_uploader.MinidumpUploadJob;
import org.chromium.components.minidump_uploader.MinidumpUploadJobImpl;

/** Class that interacts with the Android JobScheduler to upload minidumps at appropriate times. */
@NullMarked
public class ChromeMinidumpUploadJobServiceImpl extends SplitCompatMinidumpUploadJobService.Impl {
    @Override
    protected MinidumpUploadJob createMinidumpUploadJob(PersistableBundle permissions) {
        return new MinidumpUploadJobImpl(
                new ChromeMinidumpUploaderDelegate(assertNonNull(getService()), permissions));
    }
}
