// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Log;

/** Android app that uses WebView for UI Testing. */
public class WebViewUiTestActivity extends Activity {

    private static final String TAG = "WebViewUiTest";

    public static final String EXTRA_TEST_LAYOUT_FILE =
            "org.chromium.webview_ui_app.WebViewUiTestActivity.LayoutFile";

    private String mLayout;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        parseArgumentsFromIntent(getIntent());
        if (mLayout != null) {
            setContentView(getResources().getIdentifier(mLayout, "layout", getPackageName()));
        } else {
            Log.e(TAG, "Must specify activity layout via intent argument.");
        }
    }

    private void parseArgumentsFromIntent(Intent intent) {
        mLayout = intent.getStringExtra(EXTRA_TEST_LAYOUT_FILE);
    }
}
