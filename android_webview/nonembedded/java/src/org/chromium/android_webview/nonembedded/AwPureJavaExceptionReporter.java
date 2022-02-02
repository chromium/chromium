
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.nonembedded;

import org.chromium.android_webview.common.crash.CrashUploadUtil;
import org.chromium.android_webview.common.crash.SystemWideCrashDirectories;
import org.chromium.base.ContextUtils;
import org.chromium.components.crash.PureJavaExceptionReporter;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;

/* package */ class AwPureJavaExceptionReporter extends PureJavaExceptionReporter {
    private static final String WEBVIEW_CRASH_PRODUCT_NAME = "AndroidWebView";
    private static final String NONEMBEDDED_WEBVIEW_MINIDUMP_FILE_PREFIX =
            "nonembeded-webview-minidump-";

    public AwPureJavaExceptionReporter() {
        super(SystemWideCrashDirectories.getOrCreateWebViewCrashDir());
        // The reporter doesn't create a minidump if the crash dump directory doesn't exist, so make
        // sure to create it.
        // TODO(https://crbug.com/1293108): this should be shared with chrome as well and removed
        // from here.
        new File(SystemWideCrashDirectories.getOrCreateWebViewCrashDir(),
                CrashFileManager.CRASH_DUMP_DIR)
                .mkdirs();
    }

    @Override
    protected String getProductName() {
        return WEBVIEW_CRASH_PRODUCT_NAME;
    }

    @Override
    protected void uploadMinidump(File minidump) {
        CrashUploadUtil.scheduleNewJob(ContextUtils.getApplicationContext());
    }

    @Override
    protected String getMinidumpPrefix() {
        return NONEMBEDDED_WEBVIEW_MINIDUMP_FILE_PREFIX;
    }
}
