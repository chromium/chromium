// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.net.Uri;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.download.DownloadDelegate;

/** Utility class that implements DownloadDelegate. */
@NullMarked
public class DownloadDelegateImpl extends DownloadDelegate {
    public DownloadDelegateImpl() {}

    @Override
    public String remapGenericMimeType(String mimeType, String url, String filename) {
        return MimeUtils.remapGenericMimeType(mimeType, url, filename);
    }

    @Override
    public @Nullable Uri parseOriginalUrl(String originalUrl) {
        return DownloadUtils.parseOriginalUrl(originalUrl);
    }

    @Override
    public boolean isDownloadOnSDCard(String filePath) {
        return DownloadDirectoryProvider.isDownloadOnSDCard(filePath);
    }
}
