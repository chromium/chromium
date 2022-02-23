// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatIntentService;

/** See {@link IncognitoNotificationServiceImpl}. */
public class IncognitoNotificationService extends SplitCompatIntentService {
    private static final String TAG = "incognito_notification";

    @IdentifierNameString
    private static String sImplClassName =
            "org.chromium.chrome.browser.incognito.IncognitoNotificationServiceImpl";

    public IncognitoNotificationService() {
        super(sImplClassName, TAG);
    }
}
