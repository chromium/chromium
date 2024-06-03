// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatIntentService;

/**
 * Forwards startService()-type Intents to the NotificationIntentInterceptor.
 *
 * <p>While the implementation can live in the "chrome" split, this class needs to live in the
 * "base" split so it can take care of loading the "chrome" split and replace the class loader.
 *
 * <p>See {@link NotificationIntentInterceptor.ServiceImpl}.
 */
public class NotificationIntentInterceptorService extends SplitCompatIntentService {
    private static final String TAG = NotificationIntentInterceptorService.class.getSimpleName();

    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.notifications.NotificationIntentInterceptor$ServiceImpl";

    public NotificationIntentInterceptorService() {
        super(sImplClassName, TAG);
    }
}
