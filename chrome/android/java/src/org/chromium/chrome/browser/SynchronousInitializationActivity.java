// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Bundle;

import androidx.annotation.CallSuper;

import org.chromium.chrome.browser.init.ChromeBrowserInitializer;

public abstract class SynchronousInitializationActivity extends ChromeBaseAppCompatActivity {
    private static final String TAG = "SyncInitActivity";

    @CallSuper
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
    }
}
