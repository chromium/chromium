// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Bundle;

import androidx.annotation.CallSuper;

import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;

/**
 * Ensures that the native library is loaded by synchronously initializing it on creation.
 *
 * This is needed for Activities that can be started without going through the regular asynchronous
 * browser startup pathway, which could happen if the user restarted Chrome after it died in the
 * background with the Activity visible.  One example is {@link BookmarkActivity} and its kin.
 */
public abstract class SynchronousInitializationActivity extends ChromeBaseAppCompatActivity {
    @CallSuper
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Make sure the native is initialized before calling super.onCreate(), as calling
        // super.onCreate() will recreate fragments that might depend on the native code.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        super.onCreate(savedInstanceState);
    }
}
