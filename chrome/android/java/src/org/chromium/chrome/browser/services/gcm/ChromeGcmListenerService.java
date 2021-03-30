// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.annotation.SuppressLint;

import org.chromium.chrome.browser.base.SplitCompatGcmListenerService;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/**
 * See {@link ChromeGcmListenerServiceImpl}.
 * Suppressing linting as onNewToken() is implemented in base class.
 */
@SuppressLint("MissingFirebaseInstanceTokenRefresh")
public class ChromeGcmListenerService extends SplitCompatGcmListenerService {
    public ChromeGcmListenerService() {
        super(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.services.gcm.ChromeGcmListenerServiceImpl"));
    }
}
