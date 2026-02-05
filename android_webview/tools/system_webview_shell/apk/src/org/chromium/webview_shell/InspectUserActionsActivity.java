// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.widget.Button;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

/** Activity to test user actions on WebView locally */
public class InspectUserActionsActivity extends AppCompatActivity {

    @Nullable private WebView mWebView;
    private boolean mRecordingActions;
    private OnBackPressedCallback mCallback;

    private void setRecordingActions(boolean toggle) {
        if (mWebView == null) {
            return;
        }

        Button btn = findViewById(R.id.toggle_record_actions);
        mRecordingActions = toggle;
        mWebView.getSettings().setJavaScriptEnabled(toggle);
        if (toggle) {
            btn.setText(R.string.stop_record_action_button);
        } else {
            btn.setText(R.string.record_action_button);
        }
    }

    private void onToggleRecordActionsClicked(View v) {
        if (mWebView.getUrl() == null) {
            mWebView.loadUrl("chrome://user-actions/");
        }
        setRecordingActions(!mRecordingActions);
    }

    private void onReRecordActionsClicked(View v) {
        mWebView.loadUrl("chrome://user-actions/");
        setRecordingActions(true);
    }

    private void onCloseActivityClicked(View v) {
        startActivity(new Intent(InspectUserActionsActivity.this, WebViewBrowserActivity.class));
        finish();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_inspect_user_actions);

        mWebView = findViewById(R.id.inspect_user_actions_webview);
        mRecordingActions = false;

        findViewById(R.id.toggle_record_actions)
                .setOnClickListener(this::onToggleRecordActionsClicked);
        findViewById(R.id.re_record_actions).setOnClickListener(this::onReRecordActionsClicked);
        findViewById(R.id.close_activity).setOnClickListener(this::onCloseActivityClicked);

        // Stops InspectUserActionsActivity from being destroyed
        // when the user presses the back button so that
        // the webview can log user actions in the background and
        // return to original activity to view the actions

        // The user flow is:
        // 1. The user launches InspectUserActionsActivity from the menu in WebViewBrowserActivity
        // 2. The user clicks on Record User Actions
        // 3. They hit the back button, but we want to keep this activity alive so the
        // WebViewBrowserActivity is launched on top of it
        // 4. The user triggers user actions in other activities
        // 5. The user launches this activity again from the menu and is launched with CLEAR_TOP
        // which moves the background InspectUserActionsActivity to the front, killing the
        // WebViewBrowserActivity
        mCallback =
                new OnBackPressedCallback(true) {
                    @Override
                    public void handleOnBackPressed() {
                        startActivity(
                                new Intent(
                                        InspectUserActionsActivity.this,
                                        WebViewBrowserActivity.class));
                    }
                };
        getOnBackPressedDispatcher().addCallback(mCallback);
    }

    @Override
    public void onDestroy() {
        if (mCallback != null) {
            mCallback.remove();
        }

        ViewGroup viewGroup = (ViewGroup) mWebView.getParent();
        viewGroup.removeView(mWebView);
        mWebView.destroy();
        mWebView = null;

        super.onDestroy();
    }
}
