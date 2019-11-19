// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Pair;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.IntDef;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkMetaDataUtils;
import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;
import org.chromium.webapk.shell_apk.HostBrowserUtils;
import org.chromium.webapk.shell_apk.LaunchHostBrowserSelector;
import org.chromium.webapk.shell_apk.WebApkUtils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Displays splash screen. */
public class SplashActivity extends Activity {
    /** Whether {@link mSplashView} was laid out. */
    private boolean mSplashViewLaidOut;

    /** Task to screenshot and encode splash. */
    @SuppressWarnings("NoAndroidAsyncTaskCheck")
    private android.os.AsyncTask mScreenshotSplashTask;

    @IntDef({ActivityResult.NONE, ActivityResult.CANCELED, ActivityResult.IGNORE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ActivityResult {
        int NONE = 0;
        int CANCELED = 1;
        int IGNORE = 2;
    }

    private View mSplashView;
    private HostBrowserLauncherParams mParams;
    private @ActivityResult int mResult;
    private boolean mResumed;
    private boolean mPendingLaunch;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        showSplashScreen();
        final long splashAddedToLayoutTimeMs = SystemClock.elapsedRealtime();

        // On Android O+, if:
        // - Chrome is translucent
        // AND
        // - Both the WebAPK and Chrome have been killed by the Android out-of-memory killer
        // both the SplashActivity and the browser activity are created when the user selects the
        // WebAPK in Android Recents.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                && !new ComponentName(this, SplashActivity.class)
                            .equals(WebApkUtils.fetchTopActivityComponent(this, getTaskId()))) {
            return;
        }

        mPendingLaunch = true;
        selectHostBrowser(splashAddedToLayoutTimeMs);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mResult != ActivityResult.IGNORE && resultCode == Activity.RESULT_CANCELED) {
            mResult = ActivityResult.CANCELED;
        }
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);

        // Clear flag set by SplashActivity#onActivityResult()
        // The host browser activity is killed - triggering SplashActivity#onActivityResult()
        // - when SplashActivity gets a new intent because SplashActivity has launchMode
        // "singleTask".
        mResult = ActivityResult.IGNORE;

        mPendingLaunch = true;

        selectHostBrowser(-1 /* splashShownTimeMs */);
    }

    @Override
    public void onResume() {
        super.onResume();
        mResumed = true;
        if (mResult == ActivityResult.CANCELED) {
            finish();
            return;
        }

        mResult = ActivityResult.NONE;
        maybeScreenshotSplashAndLaunch();
    }

    @Override
    public void onPause() {
        super.onPause();
        mResumed = false;
    }

    @Override
    public void onDestroy() {
        SplashContentProvider.clearCache();
        if (mScreenshotSplashTask != null) {
            mScreenshotSplashTask.cancel(false);
            mScreenshotSplashTask = null;
        }
        super.onDestroy();
    }

    private void selectHostBrowser(final long splashShownTimeMs) {
        new LaunchHostBrowserSelector(this).selectHostBrowser(
                new LaunchHostBrowserSelector.Callback() {
                    @Override
                    public void onBrowserSelected(
                            String hostBrowserPackageName, boolean dialogShown) {
                        if (hostBrowserPackageName == null) {
                            finish();
                            return;
                        }
                        HostBrowserLauncherParams params =
                                HostBrowserLauncherParams.createForIntent(SplashActivity.this,
                                        getIntent(), hostBrowserPackageName, dialogShown,
                                        -1 /* launchTimeMs */, splashShownTimeMs);
                        onHostBrowserSelected(params);
                    }
                });
    }

    private void showSplashScreen() {
        Bundle metadata = WebApkUtils.readMetaData(this);
        updateStatusBar(metadata);

        int orientation = WebApkUtils.computeScreenLockOrientationFromMetaData(this, metadata);
        if (orientation != ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
            setRequestedOrientation(orientation);
        }

        mSplashView = SplashUtils.createSplashView(this);
        mSplashView.getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        if (mSplashView.getWidth() == 0 || mSplashView.getHeight() == 0) return;

                        mSplashView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                        mSplashViewLaidOut = true;
                        maybeScreenshotSplashAndLaunch();
                    }
                });
        setContentView(mSplashView);
    }

    /**
     * Sets the the color of the status bar and status bar icons.
     */
    private void updateStatusBar(Bundle metadata) {
        int statusBarColor = (int) WebApkMetaDataUtils.getLongFromMetaData(
                metadata, WebApkMetaDataKeys.THEME_COLOR, Color.WHITE);
        WebApkUtils.setStatusBarColor(getWindow(), statusBarColor);
        boolean needsDarkStatusBarIcons =
                !WebApkUtils.shouldUseLightForegroundOnBackground(statusBarColor);
        WebApkUtils.setStatusBarIconColor(
                getWindow().getDecorView().getRootView(), needsDarkStatusBarIcons);
    }

    /** Called once the host browser has been selected. */
    private void onHostBrowserSelected(HostBrowserLauncherParams params) {
        if (params == null) {
            finish();
            return;
        }

        Context appContext = getApplicationContext();

        if (!HostBrowserUtils.shouldIntentLaunchSplashActivity(params)) {
            HostBrowserLauncher.launch(this, params);
            H2OLauncher.changeEnabledComponentsAndKillShellApk(appContext,
                    new ComponentName(appContext, H2OMainActivity.class),
                    new ComponentName(appContext, H2OOpaqueMainActivity.class));
            finish();
            return;
        }

        mParams = params;
        maybeScreenshotSplashAndLaunch();
    }

    /**
     * Screenshots {@link mSplashView} if:
     * - host browser was selected
     * AND
     * - splash view was laid out
     */
    private void maybeScreenshotSplashAndLaunch() {
        // If Activity#onActivityResult() will be called, it will be called prior to the
        // activity being resumed.
        if (mParams == null || !mSplashViewLaidOut || !mResumed || !mPendingLaunch) return;
        mPendingLaunch = false;

        screenshotAndEncodeSplashInBackground();
    }

    /**
     * Launches the host browser on top of {@link SplashActivity}.
     * @param splashEncoded Encoded screenshot of {@link mSplashView}.
     * @param encodingFormat The screenshot's encoding format.
     */
    private void launch(byte[] splashEncoded, Bitmap.CompressFormat encodingFormat) {
        SplashContentProvider.cache(this, splashEncoded, encodingFormat, mSplashView.getWidth(),
                mSplashView.getHeight());
        H2OLauncher.launch(this, mParams);
        mParams = null;
    }

    /**
     * Screenshots and encodes {@link mSplashView} on a background thread.
     */
    @SuppressWarnings("NoAndroidAsyncTaskCheck")
    private void screenshotAndEncodeSplashInBackground() {
        final Bitmap bitmap = SplashUtils.screenshotView(
                mSplashView, SplashContentProvider.MAX_TRANSFER_SIZE_BYTES);
        if (bitmap == null) {
            launch(null, Bitmap.CompressFormat.PNG);
            return;
        }

        mScreenshotSplashTask =
                new android.os
                        .AsyncTask<Void, Void, Pair<byte[], Bitmap.CompressFormat>>() {
                            @Override
                            protected Pair<byte[], Bitmap.CompressFormat> doInBackground(
                                    Void... args) {
                                try (ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                                    Bitmap.CompressFormat encodingFormat =
                                            SplashUtils.selectBitmapEncoding(
                                                    bitmap.getWidth(), bitmap.getHeight());
                                    bitmap.compress(encodingFormat, 100, out);
                                    return Pair.create(out.toByteArray(), encodingFormat);
                                } catch (IOException e) {
                                }
                                return null;
                            }

                            @Override
                            protected void onPostExecute(
                                    Pair<byte[], Bitmap.CompressFormat> splashEncoded) {
                                mScreenshotSplashTask = null;
                                launch((splashEncoded == null) ? null : splashEncoded.first,
                                        (splashEncoded == null) ? Bitmap.CompressFormat.PNG
                                                                : splashEncoded.second);
                            }

                            // Do nothing if task was cancelled.
                        }
                        .executeOnExecutor(android.os.AsyncTask.THREAD_POOL_EXECUTOR);
    }
}
