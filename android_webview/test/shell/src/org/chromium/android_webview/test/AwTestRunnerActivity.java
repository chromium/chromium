// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.shell.AwShellResourceProvider;
import org.chromium.base.StrictModeContext;

/** This is a lightweight activity for tests that only require WebView functionality. */
public class AwTestRunnerActivity extends Activity {
    public static final String FLAG_HIDE_ACTION_BAR = "hide_action_bar";

    private LinearLayout mLinearLayout;
    private Intent mLastSentIntent;
    private boolean mIgnoreStartActivity;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        AwShellResourceProvider.registerResources(this);
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            AwBrowserProcess.loadLibrary(null);
        }

        mLinearLayout = new LinearLayout(this);
        mLinearLayout.setOrientation(LinearLayout.VERTICAL);
        mLinearLayout.setShowDividers(LinearLayout.SHOW_DIVIDER_MIDDLE);
        mLinearLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        hideActionBarIfNecessary();

        setContentView(mLinearLayout);
        setupEdgeToEdge();
    }

    private void hideActionBarIfNecessary() {
        Intent intent = getIntent();
        if (intent == null) return;
        Bundle extras = intent.getExtras();
        if (extras == null) return;
        Boolean hideActionBar = extras.getBoolean(FLAG_HIDE_ACTION_BAR);
        if (hideActionBar == null || !hideActionBar) return;

        // This should be called before setContentView in onCreate() to take an
        // effect.
        getActionBar().hide();
    }

    public int getRootLayoutWidth() {
        return mLinearLayout.getWidth();
    }

    /** Adds a view to the main linear layout. */
    public void addView(View view) {
        view.setLayoutParams(
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT, 1f));
        mLinearLayout.addView(view);
    }

    /** Clears the main linear layout. */
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

    private void setupEdgeToEdge() {
        ViewCompat.setOnApplyWindowInsetsListener(
                mLinearLayout,
                (v, windowInsets) -> {
                    Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
                    // Ensure the LinearLayout (container) view does not overlap with the system
                    // status bar by adjusting its top padding based on the system window insets.
                    v.setPadding(
                            v.getPaddingLeft(),
                            insets.top,
                            v.getPaddingRight(),
                            v.getPaddingBottom());

                    // Return CONSUMED to indicate we have handled the insets for this view
                    // and don't want them to be passed down to descendant views.
                    return WindowInsetsCompat.CONSUMED;
                });
    }
}
