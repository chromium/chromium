// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.shell.AwShellResourceProvider;
import org.chromium.base.StrictModeContext;

/**
 * This is a lightweight activity for tests that only require WebView functionality.
 */
public class AwTestRunnerActivity extends Activity {

    private LinearLayout mLinearLayout;
    private Intent mLastSentIntent;
    private boolean mIgnoreStartActivity;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        AwShellResourceProvider.registerResources(this);
        try (StrictModeContext ctx = StrictModeContext.allowDiskReads()) {
            AwBrowserProcess.loadLibrary(null);
        }

        mLinearLayout = new LinearLayout(this);
        mLinearLayout.setOrientation(LinearLayout.VERTICAL);
        mLinearLayout.setShowDividers(LinearLayout.SHOW_DIVIDER_MIDDLE);
        mLinearLayout.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));

        setContentView(mLinearLayout);
    }

    public int getRootLayoutWidth() {
        return mLinearLayout.getWidth();
    }

    /**
     * Adds a view to the main linear layout.
     */
    public void addView(View view) {
        view.setLayoutParams(new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT, 1f));
        mLinearLayout.addView(view);
    }

    /**
     * Clears the main linear layout.
     */
    public void removeAllViews() {
        mLinearLayout.removeAllViews();
    }

    @Override
    public void startActivity(Intent i) {
        mLastSentIntent = i;
        if (!mIgnoreStartActivity) super.startActivity(i);
    }

    public Intent getLastSentIntent() {
        return mLastSentIntent;
    }

    public void setIgnoreStartActivity(boolean ignore) {
        mIgnoreStartActivity = ignore;
    }
}
