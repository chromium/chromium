// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;

/**
 * Helper for launching a Help Center article on behalf of Download Home for a given {@link
 * Profile}.
 */
@NullMarked
public interface DownloadHelpPageLauncher {
    /**
     * Opens the URL in a Chrome Custom Tab.
     *
     * @param context The application context.
     * @param url The URL to open.
     */
    void openUrl(Context context, String url);
}
