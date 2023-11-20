// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.runtime_library.test;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;

import org.chromium.webapk.lib.runtime_library.WebApkServiceImpl;

/** Simple service which uses {@link WebApkServiceImpl} for testing. */
public class TestWebApkServiceImplWrapper extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        int smallIconId = intent.getIntExtra(WebApkServiceImpl.KEY_SMALL_ICON_ID, -1);
        int authorizedAppUid = intent.getIntExtra(WebApkServiceImpl.KEY_HOST_BROWSER_UID, -1);

        Bundle bundle = new Bundle();
        bundle.putInt(WebApkServiceImpl.KEY_SMALL_ICON_ID, smallIconId);
        bundle.putInt(WebApkServiceImpl.KEY_HOST_BROWSER_UID, authorizedAppUid);
        return (IBinder) new WebApkServiceImpl(this, bundle);
    }
}
