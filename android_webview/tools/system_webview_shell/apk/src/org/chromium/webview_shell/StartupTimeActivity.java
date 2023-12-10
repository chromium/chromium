// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.LinearLayout;

import androidx.annotation.IntDef;

import org.chromium.base.Log;

import java.util.LinkedList;

/**
 * An activity to measure the startup time of WebView in various scenarios.
 *
 * Example run:
 *   # Compile dex file for optimized run.
 *   adb shell cmd package compile -m speed -f <webview_package_name>
 *   adb shell killall org.chromium.webview_shell
 *   # Run scenario: CREATE (1). There are other scenarios you can try.
 *   adb shell am start -n org.chromium.webview_shell/.StartupTimeActivity \
 *     -a android.intent.action.VIEW --ei "target" 1
 *   adb logcat | grep WebViewShell
 *
 * Then you will see a tuple of durations in ms that the scenario blocked UI thread for each loop in
 * the logcat. Here are empirical sample results for WebView 80 on an internal Go device:
 * +-------------+-------+-----------------+
 * | target name | index | results (ms)    |
 * +-------------+-------+-----------------+
 * | DO_NOTHING  | 0     | (empty)         |
 * | CREATE      | 1     | (620, 50)       |
 * | ADD_VIEW    | 2     | (620, 150)      |
 * | LOAD        | 3     | (620, 150, 140) |
 * | WORKAROUND  | 4     | (220, 155)      |
 * +-------------+-------+-----------------+
 *
 * Note that you need to run this multiple times and ignore the first few runs
 * to get an accurate measurement.
 */
public class StartupTimeActivity extends Activity {
    private static final String TAG = "WebViewShell";
    // Only records the tasks that affect the rendering performance of 30 FPS.
    private static final long MIN_TIME_TO_RECORD_MS = 33;
    private static final long TIME_TO_FINISH_APP_MS = 5000;
    private static final long TIME_TO_WAIT_BEFORE_START_MS = 2000;

    private static final String TARGET_KEY = "target";

    private LinkedList<Long> mEventQueue = new LinkedList<>();

    private boolean mFinished;
    // Keep track of the time that the last task was run.
    private long mLastTaskTimeMs = -1;

    private LinearLayout mLayout;
    private WebView mWebView;

    private Handler mHandler;

    @IntDef({Target.DO_NOTHING, Target.CREATE, Target.ADD_VIEW, Target.LOAD, Target.WORKAROUND})
    @interface Target {
        int DO_NOTHING = 0;
        int CREATE = 1;
        int ADD_VIEW = 2;
        int LOAD = 3;
        int WORKAROUND = 4;
    }

    private Runnable mUiBlockingTaskTracker =
            new Runnable() {
                @Override
                public void run() {
                    if (mFinished) return;
                    long now = System.currentTimeMillis();
                    if (mLastTaskTimeMs != -1) {
                        // The diff between current time and last task time is approximately
                        // the time other UI tasks were run.
                        long gap = now - mLastTaskTimeMs;
                        if (gap > MIN_TIME_TO_RECORD_MS) {
                            mEventQueue.add(gap);
                        }
                    }
                    mLastTaskTimeMs = now;
                    // Self-posting the current task to track future UI blocking tasks.
                    mHandler.post(mUiBlockingTaskTracker);
                }
            };

    private Runnable mFinishTask =
            new Runnable() {
                @Override
                public void run() {
                    mFinished = true;
                    String res = TextUtils.join(", ", mEventQueue);
                    Log.i(TAG, "UI blocking times in startup (ms): " + res);
                    if (mWebView != null) {
                        // Remove webview from view hierarchy while preventing NPE.
                        mLayout.removeAllViews();
                        mWebView.destroy();
                        mWebView = null;
                    }
                    finish();
                }
            };

    private @Target int getTarget() {
        final int defaultTarget = Target.CREATE;
        Intent intent = getIntent();
        if (intent == null) return defaultTarget;
        Bundle extras = intent.getExtras();
        if (extras == null) return defaultTarget;
        return extras.getInt(TARGET_KEY, defaultTarget);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setTitle(getResources().getString(R.string.title_activity_startup_time));
        mLayout = new LinearLayout(this);
        setContentView(mLayout);
        mHandler = new Handler();
        @Target int target = getTarget();
        Log.i(TAG, "Target: " + target);
        // There are other posted tasks caused by the activity. Give it some delay to avoid
        // recording them.
        mHandler.postDelayed(
                () -> {
                    runScenario(target);
                },
                TIME_TO_WAIT_BEFORE_START_MS);
    }

    private void runScenario(@Target int target) {
        mUiBlockingTaskTracker.run();
        switch (target) {
            case Target.DO_NOTHING:
                {
                    // This is the baseline. It should output an empty result.
                    break;
                }
            case Target.CREATE:
                {
                    mWebView = new WebView(this);
                    break;
                }
            case Target.ADD_VIEW:
                {
                    mWebView = new WebView(this);
                    mLayout.addView(mWebView);
                    break;
                }
            case Target.LOAD:
                {
                    mWebView = new WebView(this);
                    mLayout.addView(mWebView);
                    mWebView.loadUrl("about:blank");
                    break;
                }
            case Target.WORKAROUND:
                {
                    // This is a useful hack to run some of the startup tasks in a background
                    // thread to reduce the UI thread blocking time.
                    Thread t =
                            new Thread(
                                    () -> {
                                        WebSettings.getDefaultUserAgent(this);
                                        // Note that there are some UI tasks caused by
                                        // getDefaultUserAgent().
                                        // But this will ensure that new WebView() can be run after
                                        // those UI tasks are run first.
                                        mHandler.post(
                                                () -> {
                                                    mWebView = new WebView(this);
                                                });
                                    });
                    t.start();
                    break;
                }
        }
        mHandler.postDelayed(mFinishTask, TIME_TO_FINISH_APP_MS);
    }
}
