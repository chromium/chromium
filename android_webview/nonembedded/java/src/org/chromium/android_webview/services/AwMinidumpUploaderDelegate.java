// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.content.Context;
import android.net.ConnectivityManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.nonembedded.crash.SystemWideCrashDirectories;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.version_info.Channel;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.components.minidump_uploader.MinidumpUploaderDelegate;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.NetworkPermissionUtil;

import java.io.File;
import java.util.Random;

/** Android Webview-specific implementations for minidump uploading logic. */
public class AwMinidumpUploaderDelegate implements MinidumpUploaderDelegate {
    // Sample 1% of crashes for stable WebView channel.
    public static final int CRASH_DUMP_PERCENTAGE_FOR_STABLE = 1;

    private final ConnectivityManager mConnectivityManager;

    private boolean mPermittedByUser;

    private SamplingDelegate mSamplingDelegate;

    /**
     * A delegate to provide the required information to decide whether a crash is sampled or not.
     * This is to allow injecting delegates for testing.
     */
    @VisibleForTesting
    public static interface SamplingDelegate {
        public int getChannel();

        public int getRandomSample();
    }

    @VisibleForTesting
    public AwMinidumpUploaderDelegate(SamplingDelegate samplingDelegate) {
        mConnectivityManager =
                (ConnectivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.CONNECTIVITY_SERVICE);

        mSamplingDelegate = samplingDelegate;
    }

    public AwMinidumpUploaderDelegate() {
        this(
                new SamplingDelegate() {
                    private Random mRandom = new Random();

                    @Override
                    public int getChannel() {
                        return VersionConstants.CHANNEL;
                    }

                    @Override
                    public int getRandomSample() {
                        return mRandom.nextInt(100);
                    }
                });
    }

    @Override
    public File getCrashParentDir() {
        return SystemWideCrashDirectories.getWebViewCrashDir();
    }

    @Override
    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
        return new CrashReportingPermissionManager() {
            @Override
            public boolean isClientInSampleForCrashes() {
                // Downsample unknown channel as a precaution in case it ends up being shipped.
                if (mSamplingDelegate.getChannel() == Channel.STABLE
                        || mSamplingDelegate.getChannel() == Channel.DEFAULT) {
                    return mSamplingDelegate.getRandomSample() < CRASH_DUMP_PERCENTAGE_FOR_STABLE;
                }

                return true;
            }

            @Override
            public boolean isNetworkAvailableForCrashUploads() {
                // Note that this is the same critierion that the JobScheduler uses to schedule the
                // job. JobScheduler will call onStopJob causing our upload to be interrupted when
                // our network requirements no longer hold.
                return NetworkPermissionUtil.isNetworkUnmetered(mConnectivityManager);
            }

            @Override
            public boolean isUsageAndCrashReportingPermittedByPolicy() {
                // Metrics reporting can only be disabled by the user and the app.
                // Return true since Chrome policy doesn't apply to WebView.
                return true;
            }

            @Override
            public boolean isUsageAndCrashReportingPermittedByUser() {
                return mPermittedByUser;
            }

            @Override
            public boolean isUploadEnabledForTests() {
                // Note that CommandLine/CommandLineUtil are not thread safe. They are initialized
                // on the main thread, but before the current worker thread started - so this thread
                // will have seen the initialization of the CommandLine.
                return CommandLine.getInstance()
                        .hasSwitch(BaseSwitches.ENABLE_CRASH_REPORTER_FOR_TESTING);
            }
        };
    }

    @Override
    public void prepareToUploadMinidumps(final Runnable startUploads) {
        PlatformServiceBridge.getInstance()
                .queryMetricsSetting(
                        enabled -> {
                            ThreadUtils.assertOnUiThread();
                            mPermittedByUser = Boolean.TRUE.equals(enabled);
                            startUploads.run();
                        });
    }

    @Override
    public void recordUploadSuccess(File minidump) {}

    @Override
    public void recordUploadFailure(File minidump) {}
}
