// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;

/** Helper class to build default or base {@link DownloadManagerUiConfig.Builder} instances. */
@NullMarked
public class DownloadManagerUiConfigHelper {
    private DownloadManagerUiConfigHelper() {}

    /** Creates a {@link DownloadManagerUiConfig.Builder} based on feature flags. */
    public static DownloadManagerUiConfig.Builder fromFlags(Context context) {
        boolean showDangerousItems =
                ChromeFeatureList.sMaliciousApkDownloadCheck.isEnabled()
                        && !ChromeFeatureList.sMaliciousApkDownloadCheckTelemetryOnly.getValue();
        boolean isLff = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
        return new DownloadManagerUiConfig.Builder()
                .setShowDangerousItems(showDangerousItems)
                .setSupportsGrouping(true)
                .setInlineSearchBar(isLff)
                .setAutoFocusSearchBox(isLff && DeviceInput.supportsKeyboard());
    }
}
