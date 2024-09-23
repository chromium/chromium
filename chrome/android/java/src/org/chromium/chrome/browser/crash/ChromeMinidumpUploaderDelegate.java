// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.crash;

import android.content.Context;
import android.net.ConnectivityManager;
import android.os.PersistableBundle;

import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.components.minidump_uploader.MinidumpUploaderDelegate;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.NetworkPermissionUtil;

import java.io.File;

/** Chrome-specific implementations for minidump uploading logic. */
public class ChromeMinidumpUploaderDelegate implements MinidumpUploaderDelegate {
    // PersistableBundle keys:
    static final String IS_CLIENT_IN_SAMPLE_FOR_CRASHES = "isClientInSampleForCrashes";
    static final String IS_UPLOAD_ENABLED_FOR_TESTS = "isUploadEnabledForTests";

    /** The application context in which minidump uploads are running. */
    private final Context mContext;

    /**
     * The cached crash reporting permissions. These are cached because the upload job might run
     * outside of a context in which the original permissions are easily accessible.
     */
    private final PersistableBundle mPermissions;

    /** The system connectivity manager service, used to determine the network state. */
    private final ConnectivityManager mConnectivityManager;

    /**
     * Constructs a new Chrome-specific minidump uploader delegate.
     * @param context The application context in which minidump uploads are running.
     * @param permissions The cached crash reporting permissions.
     */
    ChromeMinidumpUploaderDelegate(Context context, PersistableBundle permissions) {
        mContext = context;
        mPermissions = permissions;
        mConnectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    @Override
    public File getCrashParentDir() {
        return mContext.getCacheDir();
    }

    @Override
    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
        return new CrashReportingPermissionManager() {
            @Override
            public boolean isClientInSampleForCrashes() {
                return mPermissions.getBoolean(IS_CLIENT_IN_SAMPLE_FOR_CRASHES, true);
            }

            @Override
            public boolean isNetworkAvailableForCrashUploads() {
                return NetworkPermissionUtil.isNetworkUnmetered(mConnectivityManager);
            }

            @Override
            public boolean isUsageAndCrashReportingPermittedByPolicy() {
                return PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByPolicy();
            }

            @Override
            public boolean isUsageAndCrashReportingPermittedByUser() {
                return PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser();
            }

            @Override
            public boolean isUploadEnabledForTests() {
                return mPermissions.getBoolean(IS_UPLOAD_ENABLED_FOR_TESTS, false);
            }
        };
    }

    @Override
    public void prepareToUploadMinidumps(final Runnable startUploads) {
        startUploads.run();
    }

    @Override
    public void recordUploadSuccess(File minidump) {
        MinidumpUploadServiceImpl.incrementCrashSuccessUploadCount(minidump.getAbsolutePath());
    }

    @Override
    public void recordUploadFailure(File minidump) {
        MinidumpUploadServiceImpl.incrementCrashFailureUploadCount(minidump.getAbsolutePath());
    }
}
