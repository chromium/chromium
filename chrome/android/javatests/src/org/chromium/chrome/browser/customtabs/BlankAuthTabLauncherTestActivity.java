// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.net.Uri;

import androidx.activity.result.ActivityResultLauncher;
import androidx.browser.auth.AuthTabIntent;

import org.chromium.base.Callback;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Simplified activity to launch Auth Tabs for testing. */
public class BlankAuthTabLauncherTestActivity extends BlankUiTestActivity {
    private final ActivityResultLauncher<Intent> mLauncher =
            AuthTabIntent.registerActivityResultLauncher(this, this::handleAuthResult);
    private Callback<AuthTabIntent.AuthResult> mAuthCallback;

    /**
     * Launches an Auth Tab.
     *
     * @param packageName The package name of the browser to launch.
     * @param url The URL to load in the Auth Tab.
     * @param redirectScheme The redirect scheme to use for the Auth Tab.
     */
    public void launchAuthTab(
            String packageName,
            String url,
            String redirectScheme,
            Callback<AuthTabIntent.AuthResult> callback) {
        mAuthCallback = callback;
        AuthTabIntent intent = new AuthTabIntent.Builder().build();
        intent.intent.setPackage(packageName);
        intent.launch(mLauncher, Uri.parse(url), redirectScheme);
    }

    private void handleAuthResult(AuthTabIntent.AuthResult result) {
        mAuthCallback.onResult(result);
    }
}
