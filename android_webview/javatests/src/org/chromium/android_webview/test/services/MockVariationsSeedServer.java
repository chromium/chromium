// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import android.content.Intent;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;

import org.chromium.android_webview.common.services.IVariationsSeedServer;
import org.chromium.android_webview.services.VariationsSeedServer;
import org.chromium.base.test.util.CallbackHelper;

/**
 * VariationsSeedServer is a bound service that shares the Variations seed with all the WebViews
 * on the system. A WebView will bind and call getSeed, passing a file descriptor to which the
 * service should write the seed.
 */
public class MockVariationsSeedServer extends VariationsSeedServer {
    private static CallbackHelper sOnSeedRequested = new CallbackHelper();

    public static CallbackHelper getRequestHelper() {
        return sOnSeedRequested;
    }

    private final IVariationsSeedServer.Stub mMockBinder = new IVariationsSeedServer.Stub() {
        @Override
        public void getSeed(ParcelFileDescriptor newSeedFile, long oldSeedDate) {
            sOnSeedRequested.notifyCalled();
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mMockBinder;
    }
}
