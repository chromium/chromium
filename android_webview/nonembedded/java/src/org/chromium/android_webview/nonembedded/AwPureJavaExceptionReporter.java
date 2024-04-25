// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.nonembedded;

import org.chromium.android_webview.nonembedded.crash.CrashUploadUtil;
import org.chromium.android_webview.nonembedded.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.services.CrashLoggingUtils;
import org.chromium.base.ContextUtils;
import org.chromium.components.crash.PureJavaExceptionReporter;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;

/* package */ class AwPureJavaExceptionReporter extends PureJavaExceptionReporter {
    private static final String WEBVIEW_CRASH_PRODUCT_NAME = "AndroidWebView";
    private static final String NONEMBEDDED_WEBVIEW_MINIDUMP_FILE_PREFIX =
            "nonembeded-webview-minidump-";
    private static boolean sCrashDirMade;

    public AwPureJavaExceptionReporter() {
        super(/* attachLogcat= */ false);
    }

    @Override
    protected void createReportFile() {
        super.createReportFile();
        if (mMinidumpFile != null) {
            File jsonLogFile =
                    SystemWideCrashDirectories.createCrashJsonLogFile(mMinidumpFile.getName());
            CrashLoggingUtils.writeCrashInfoToLogFile(jsonLogFile, mMinidumpFile, mReportContent);
        }
    }

    @Override
    protected File getCrashFilesDirectory() {
        if (!sCrashDirMade) {
            // The reporter doesn't create a minidump if the crash dump directory doesn't exist, so
            // make sure to create it.
            // TODO(crbug.com/40213369): this should be shared with chrome as well and
            // removed from here.
            new File(
                            SystemWideCrashDirectories.getOrCreateWebViewCrashDir(),
                            CrashFileManager.CRASH_DUMP_DIR)
                    .mkdirs();
            sCrashDirMade = true;
        }
        return SystemWideCrashDirectories.getWebViewCrashDir();
    }

    @Override
    protected String getProductName() {
        return WEBVIEW_CRASH_PRODUCT_NAME;
    }

    @Override
    protected void uploadMinidump(File minidump) {
        // The minidump file will only be ready for upload if PureJavaExceptionReporter attached
        // logcat successfully, WebView should upload it even if attaching logcat was failed.
        if (!CrashFileManager.isReadyUploadForFirstTime(minidump)) {
            CrashFileManager.trySetReadyForUpload(minidump);
        }
        CrashUploadUtil.scheduleNewJob(ContextUtils.getApplicationContext());
    }

    @Override
    protected String getMinidumpPrefix() {
        return NONEMBEDDED_WEBVIEW_MINIDUMP_FILE_PREFIX;
    }
}
