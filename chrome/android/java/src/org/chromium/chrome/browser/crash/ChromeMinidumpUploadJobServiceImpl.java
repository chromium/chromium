// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.os.PersistableBundle;

import org.chromium.components.minidump_uploader.MinidumpUploadJob;
import org.chromium.components.minidump_uploader.MinidumpUploadJobImpl;

/** Class that interacts with the Android JobScheduler to upload minidumps at appropriate times. */
public class ChromeMinidumpUploadJobServiceImpl extends ChromeMinidumpUploadJobService.Impl {
    @Override
    protected MinidumpUploadJob createMinidumpUploadJob(PersistableBundle permissions) {
        return new MinidumpUploadJobImpl(
                new ChromeMinidumpUploaderDelegate(getService(), permissions));
    }
}
