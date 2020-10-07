// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import org.chromium.chrome.browser.base.SplitCompatIntentService;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** See {@link IncognitoNotificationServiceImpl}. */
public class IncognitoNotificationService extends SplitCompatIntentService {
    private static final String TAG = "incognito_notification";
    public IncognitoNotificationService() {
        super(SplitCompatUtils.getIdentifierName(
                      "org.chromium.chrome.browser.incognito.IncognitoNotificationServiceImpl"),
                TAG);
    }
}
