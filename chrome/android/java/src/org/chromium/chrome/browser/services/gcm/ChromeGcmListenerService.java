// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.annotation.SuppressLint;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatGcmListenerService;

/**
 * See {@link ChromeGcmListenerServiceImpl}.
 * Suppressing linting as onNewToken() is implemented in base class.
 */
@SuppressLint("MissingFirebaseInstanceTokenRefresh")
public class ChromeGcmListenerService extends SplitCompatGcmListenerService {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.services.gcm.ChromeGcmListenerServiceImpl";

    public ChromeGcmListenerService() {
        super(sImplClassName);
    }
}
