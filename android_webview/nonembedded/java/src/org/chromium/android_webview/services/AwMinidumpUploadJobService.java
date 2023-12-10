// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.os.PersistableBundle;

import org.chromium.components.minidump_uploader.MinidumpUploadJob;
import org.chromium.components.minidump_uploader.MinidumpUploadJobImpl;
import org.chromium.components.minidump_uploader.MinidumpUploadJobService;

/** Class that interacts with the Android JobScheduler to upload Minidumps at appropriate times. */
// OBS: This class needs to be public to be started from android.app.ActivityThread.
public class AwMinidumpUploadJobService extends MinidumpUploadJobService {
    @Override
    protected MinidumpUploadJob createMinidumpUploadJob(PersistableBundle unusedExtras) {
        return new MinidumpUploadJobImpl(new AwMinidumpUploaderDelegate());
    }
}
