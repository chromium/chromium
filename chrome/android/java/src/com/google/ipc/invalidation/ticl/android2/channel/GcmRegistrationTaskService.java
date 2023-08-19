// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.ipc.invalidation.ticl.android2.channel;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.GcmTaskService;
import com.google.android.gms.gcm.TaskParams;

/**
 * A class with this name was part of the cacheinvalidation library, which isn't used anymore and
 * has been deleted. However, this service is exported in the AndroidManifest.xml and thus is part
 * of Chrome's public API, so we need to keep this placeholder class around.
 */
public class GcmRegistrationTaskService extends GcmTaskService {
    @Override
    public int onRunTask(TaskParams params) {
        return GcmNetworkManager.RESULT_FAILURE;
    }
}
