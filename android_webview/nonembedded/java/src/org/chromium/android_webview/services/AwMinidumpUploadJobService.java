// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.os.PersistableBundle;

import org.chromium.components.minidump_uploader.MinidumpUploadJobService;
import org.chromium.components.minidump_uploader.MinidumpUploader;
import org.chromium.components.minidump_uploader.MinidumpUploaderImpl;

/**
 * Class that interacts with the Android JobScheduler to upload Minidumps at appropriate times.
 */
// OBS: This class needs to be public to be started from android.app.ActivityThread.
public class AwMinidumpUploadJobService extends MinidumpUploadJobService {
    @Override
    protected MinidumpUploader createMinidumpUploader(PersistableBundle unusedExtras) {
        return new MinidumpUploaderImpl(new AwMinidumpUploaderDelegate());
    }
}
