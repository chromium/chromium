// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.chromium.base.Log;

/**
 * Activity for displaying WebView OSS licenses.
 * This is compiled into the stub WebView and so should not depend on any classes from Chromium
 * other than LicenseContentProvider.
 */
public class LicenseActivity extends Activity {
    private static final String TAG = "WebView";

    private static final String LICENSES_URI_SUFFIX = "LicenseContentProvider/webview_licenses";
    private static final String LICENSES_CONTENT_TYPE = "text/html";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final String packageName = getPackageName();
        final Intent intent = new Intent(Intent.ACTION_VIEW);
        final String licenseUri =
                String.format("content://%s.%s", packageName, LICENSES_URI_SUFFIX);
        intent.setDataAndType(Uri.parse(licenseUri), LICENSES_CONTENT_TYPE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        // Resources are accessed via getIdentifier because resource ids in the standalone system
        // webview apk have incorrect package_ids. Accessing resources via getIdentifier needs to be
        // whitelisted in //android_webview/aapt2.config. see https://crbug.com/894208
        final int titleId =
                getResources().getIdentifier("license_activity_title", "string", packageName);
        if (titleId != 0) {
            intent.putExtra(Intent.EXTRA_TITLE, getString(titleId));
        }
        intent.setPackage("com.android.htmlviewer");

        try {
            startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Failed to find viewer", e);
        }
        finish();
    }
}
