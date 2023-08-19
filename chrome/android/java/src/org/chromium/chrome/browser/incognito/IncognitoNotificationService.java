// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatIntentService;

/** See {@link IncognitoNotificationServiceImpl}. */
public class IncognitoNotificationService extends SplitCompatIntentService {
    private static final String TAG = "incognito_notification";

    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.incognito.IncognitoNotificationServiceImpl";

    public IncognitoNotificationService() {
        super(sImplClassName, TAG);
    }
}
