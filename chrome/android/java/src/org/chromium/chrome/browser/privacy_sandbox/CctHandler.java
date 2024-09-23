// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.LaunchIntentDispatcher;

/** Handles the requests which should be handled by CCT (Custom Chrome Tabs). */
public class CctHandler {
    private Intent mIntent;
    private final Context mContext;

    /**
     * Constructs a new CctHandler with the given context.
     *
     * @param context The application context.
     */
    public CctHandler(Context context) {
        mContext = context;
    }

    /**
     * Prepares the intent to open the specified URL in a Custom Chrome Tab.
     *
     * @param url The URL to open.
     * @return This CctHandler instance for chaining method calls.
     */
    public CctHandler prepareIntent(String url) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        mIntent =
                LaunchIntentDispatcher.createCustomTabActivityIntent(
                        mContext, customTabIntent.intent);
        mIntent.setPackage(mContext.getPackageName());
        mIntent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        return this;
    }

    /**
     * Opens the URL prepared by {@link CctHandler#prepareIntent(String)} in a Custom Chrome Tab.
     *
     * @return This CctHandler instance for chaining method calls (though likely not needed after
     *     this).
     * @throws AssertionError If {@link CctHandler#prepareIntent(String)} was not called before.
     */
    public CctHandler openUrlInCct() {
        assert mIntent != null;
        IntentUtils.addTrustedIntentExtras(mIntent);
        IntentUtils.safeStartActivity(mContext, mIntent);
        return this;
    }

    Intent getIntent() {
        return mIntent;
    }
}
