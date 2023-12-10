// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test.services;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.ResultReceiver;

import org.junit.Assert;

import org.chromium.android_webview.services.ComponentsProviderPathUtil;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.CallbackHelper;

import java.io.File;

/** Mock service that feeds mock data to components download directory. */
public class MockAwComponentUpdateService extends Service {
    public static ResultReceiver sFinishCallback;
    private static CallbackHelper sServiceFinishedCallbackHelper = new CallbackHelper();
    public static final String MOCK_COMPONENT_A_NAME = "MockComponent A";
    public static final String MOCK_COMPONENT_A_VERSION = "1.1.1.1";
    public static final String MOCK_COMPONENT_B_NAME = "MockComponent B";
    public static final String MOCK_COMPONENT_B_VERSION = "2.2.2.2";

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    public static CallbackHelper getServiceFinishedCallbackHelper() {
        return sServiceFinishedCallbackHelper;
    }

    public static void sendResultReceiverCallback() {
        Assert.assertNotNull(sFinishCallback);
        sFinishCallback.send(0, null);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        sFinishCallback = IntentUtils.safeGetParcelableExtra(intent, "SERVICE_FINISH_CALLBACK");
        File componentsDownloadDir =
                new File(ComponentsProviderPathUtil.getComponentUpdateServiceDirectoryPath());
        new File(componentsDownloadDir, MOCK_COMPONENT_A_NAME + "/" + MOCK_COMPONENT_A_VERSION)
                .mkdirs();
        new File(componentsDownloadDir, MOCK_COMPONENT_B_NAME + "/" + MOCK_COMPONENT_B_VERSION)
                .mkdirs();
        stopSelf(startId);
        sServiceFinishedCallbackHelper.notifyCalled();
        return START_STICKY;
    }
}
