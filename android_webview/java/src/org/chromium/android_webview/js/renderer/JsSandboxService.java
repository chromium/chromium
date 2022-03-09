// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js.renderer;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.android_webview.js.common.IJsSandboxContext;
import org.chromium.android_webview.js.common.IJsSandboxService;

/**
 * Service that creates a context for Javascript execution. TODO(crbug.com/1297672): Currently this
 * is just meant to be a tracer to define the end to end flow and does not do anything useful.
 */
public class JsSandboxService extends Service {
    private static final String TAG = "JsSandboxService";

    private final IJsSandboxService.Stub mBinder = new IJsSandboxService.Stub() {
        @Override
        public IJsSandboxContext createContext() {
            return new JsSandboxContext();
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
