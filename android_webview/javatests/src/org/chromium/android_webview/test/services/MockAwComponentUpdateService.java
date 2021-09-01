// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test.services;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.ResultReceiver;

import org.junit.Assert;

import org.chromium.android_webview.services.ComponentsProviderPathUtil;

import java.io.File;

/**
 * Mock service that feeds mock data to components download directory.
 */
public class MockAwComponentUpdateService extends Service {
    private ResultReceiver mFinishCallback;
    public static final String MOCK_COMPONENT_A_NAME = "MockComponent A";
    public static final String MOCK_COMPONENT_A_VERSION = "1.1.1.1";
    public static final String MOCK_COMPONENT_B_NAME = "MockComponent B";
    public static final String MOCK_COMPONENT_B_VERSION = "2.2.2.2";

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        mFinishCallback = intent.getParcelableExtra("SERVICE_FINISH_CALLBACK");
        File componentsDownloadDir =
                new File(ComponentsProviderPathUtil.getComponentUpdateServiceDirectoryPath());
        new File(componentsDownloadDir, MOCK_COMPONENT_A_NAME + "/" + MOCK_COMPONENT_A_VERSION)
                .mkdirs();
        new File(componentsDownloadDir, MOCK_COMPONENT_B_NAME + "/" + MOCK_COMPONENT_B_VERSION)
                .mkdirs();

        Assert.assertNotNull(mFinishCallback);
        mFinishCallback.send(0, null);
        mFinishCallback = null;
        stopSelf(startId);

        return START_STICKY;
    }
}
