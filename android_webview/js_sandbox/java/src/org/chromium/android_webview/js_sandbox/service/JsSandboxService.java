// Copyright 2022 The Chromium Authors
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

import java.util.Arrays;
import java.util.List;

/** Service that creates a Isolate for Javascript execution. */
public class JsSandboxService extends Service {
    private static final String TAG = "JsSandboxService";

    private static final List<String> SUPPORTED_FEATURES = Arrays.asList(
            IJsSandboxService.ISOLATE_TERMINATION, IJsSandboxService.WASM_FROM_ARRAY_BUFFER,
            IJsSandboxService.ISOLATE_MAX_HEAP_SIZE_LIMIT);

    private final IJsSandboxService.Stub mBinder = new IJsSandboxService.Stub() {
        @Override
        public IJsSandboxIsolate createIsolate() {
            return new JsSandboxIsolate();
        }

        @Override
        public IJsSandboxIsolate createIsolateWithMaxHeapSizeBytes(long maxHeapSizeBytes) {
            return new JsSandboxIsolate(maxHeapSizeBytes);
        }

        @Override
        public List<String> getSupportedFeatures() {
            return SUPPORTED_FEATURES;
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
