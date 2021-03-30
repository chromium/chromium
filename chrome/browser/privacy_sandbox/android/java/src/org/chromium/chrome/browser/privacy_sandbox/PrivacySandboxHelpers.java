// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.Intent;

/**
 * This interface allows passing non-modularized dependencies to Privacy Sandbox.
 */
public interface PrivacySandboxHelpers {
    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using
     * {@link org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/1181700): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }

    /**
     * Functional interface to append trusted extras to the given intent, e.g. by using
     * {@link org.chromium.chrome.browser.IntentHandler#addTrustedIntentExtras(Intent)}.
     * TODO(crbug.com/1181700): Update when IntentHandler is (partially-)modularized.
     */
    interface TrustedIntentHelper {
        /** @see org.chromium.chrome.browser.IntentHandler#addTrustedIntentExtras(Intent) */
        void addTrustedIntentExtras(Intent intent);
    }
}
