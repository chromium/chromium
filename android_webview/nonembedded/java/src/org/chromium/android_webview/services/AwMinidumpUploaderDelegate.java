// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.content.Context;
import android.net.ConnectivityManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.crash.SystemWideCrashDirectories;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.components.minidump_uploader.MinidumpUploaderDelegate;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.NetworkPermissionUtil;

import java.io.File;

/**
 * Android Webview-specific implementations for minidump uploading logic.
 */
public class AwMinidumpUploaderDelegate implements MinidumpUploaderDelegate {
    private final ConnectivityManager mConnectivityManager;

    private boolean mPermittedByUser;

    @VisibleForTesting
    public AwMinidumpUploaderDelegate() {
        mConnectivityManager =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
    }

    @Override
    public File getCrashParentDir() {
        return SystemWideCrashDirectories.getWebViewCrashDir();
    }

    @Override
    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
        return new CrashReportingPermissionManager() {
            @Override
            public boolean isClientInMetricsSample() {
                // We will check whether the client is in the metrics sample before
                // generating a minidump - so if no minidump is generated this code will
                // never run and we don't need to check whether we are in the sample.
                // TODO(gsennton): when we switch to using Finch for this value we should use the
                // Finch value here as well.
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
            public boolean isUsageAndCrashReportingPermittedByUser() {
                return mPermittedByUser;
            }
            @Override
            public boolean isUploadEnabledForTests() {
                // Note that CommandLine/CommandLineUtil are not thread safe. They are initialized
                // on the main thread, but before the current worker thread started - so this thread
                // will have seen the initialization of the CommandLine.
                return CommandLine.getInstance().hasSwitch(
                        CommandLineUtil.CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH);
            }
        };
    }

    @Override
    public void prepareToUploadMinidumps(final Runnable startUploads) {
        PlatformServiceBridge.getInstance().queryMetricsSetting(enabled -> {
            ThreadUtils.assertOnUiThread();
            mPermittedByUser = enabled;
            startUploads.run();
        });
    }

    @Override
    public void recordUploadSuccess(File minidump) {}

    @Override
    public void recordUploadFailure(File minidump) {}
}
