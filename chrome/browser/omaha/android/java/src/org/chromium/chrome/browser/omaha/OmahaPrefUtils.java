// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;

public class OmahaPrefUtils {
    // Flags for retrieving the OmahaClient's state after it's written to disk.
    // The PREF_PACKAGE doesn't match the current OmahaClient package for historical reasons.
    static final String PREF_PACKAGE = "com.google.android.apps.chrome.omaha";
    static final String PREF_INSTALL_SOURCE = "installSource";
    static final String PREF_LATEST_VERSION = "latestVersion";
    static final String PREF_MARKET_URL = "marketURL";
    static final String PREF_SERVER_DATE = "serverDate";
    static final String PREF_PERSISTED_REQUEST_ID = "persistedRequestID";
    static final String PREF_SEND_INSTALL_EVENT = "sendInstallEvent";
    static final String PREF_TIMESTAMP_FOR_NEW_REQUEST = "timestampForNewRequest";
    static final String PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT = "timestampForNextPostAttempt";
    static final String PREF_TIMESTAMP_OF_INSTALL = "timestampOfInstall";
    static final String PREF_TIMESTAMP_OF_REQUEST = "timestampOfRequest";

    /** Returns the Omaha SharedPreferences. */
    public static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);
    }
}
