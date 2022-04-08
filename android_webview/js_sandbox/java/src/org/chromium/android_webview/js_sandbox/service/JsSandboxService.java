// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxService;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;

/** Service that creates a Isolate for Javascript execution. */
public class JsSandboxService extends Service {
    private static final String TAG = "JsSandboxService";

    private final IJsSandboxService.Stub mBinder = new IJsSandboxService.Stub() {
        @Override
        public IJsSandboxIsolate createIsolate() {
            return new JsSandboxIsolate();
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onCreate() {
        ensureNativeInitialized();
        JsSandboxIsolate.initializeEnvironment();
    }

    private void ensureNativeInitialized() {
        if (LibraryLoader.getInstance().isInitialized()) {
            return;
        }
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_WEBVIEW_CHILD);
        LibraryLoader.getInstance().ensureInitialized();
    }
}
