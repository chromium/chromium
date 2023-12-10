// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

/** Helper class to build default or base {@link DownloadManagerUiConfig.Builder} instances. */
public class DownloadManagerUiConfigHelper {
    private DownloadManagerUiConfigHelper() {}

    /** Creates a {@link DownloadManagerUiConfig.Builder} based on feature flags. */
    public static DownloadManagerUiConfig.Builder fromFlags() {
        return new DownloadManagerUiConfig.Builder().setSupportsGrouping(true);
    }
}
