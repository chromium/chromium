// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.Intent;

/** This interface allows passing non-modularized dependencies to Privacy Sandbox. */
public interface PrivacySandboxHelpers {
    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/40751023): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }
}
