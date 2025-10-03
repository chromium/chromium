// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.net.Uri;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/**
 * Represents the LaunchParams values to be sent to a web app on app launch and whether the launch
 * triggered a navigation or not.
 */
@NullMarked
public class WebAppLaunchParams {
    /**
     * Whether this launch triggered a navigation that needs to be awaited before sending the launch
     * params to the document.
     */
    public final boolean newNavigationStarted;

    /** The app being launched, used for scope validation. */
    public final String packageName;

    /**
     * The URL the web app was launched with. Note that redirects may cause us to enqueue in a
     * different URL, we still report the original launch target URL in the launch params.
     */
    public final String targetUrl;

    /**
     * The array of file URIs, if the web app was launched by opening one or multiple files. The
     * URIs will be included in the launch params.
     */
    public final String[] fileUris;

    public WebAppLaunchParams(
            boolean newNavigationStarted,
            String targetUrl,
            String packageName,
            @Nullable List<Uri> fileUris) {
        this.newNavigationStarted = newNavigationStarted;
        this.targetUrl = targetUrl;
        this.packageName = packageName;
        this.fileUris = getFileUrisArray(fileUris);
    }

    private String[] getFileUrisArray(@Nullable List<Uri> urisList) {
        if (urisList == null) {
            return new String[0];
        }

        String[] urisArray = new String[urisList.size()];
        int i = 0;
        for (Uri uri : urisList) {
            urisArray[i] = uri.toString();
            i++;
        }

        return urisArray;
    }
}
