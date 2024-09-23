// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.content.Context;
import android.content.Intent;

/** This class allows passing non-modularized dependencies to the Enhanced Protection Fragment. */
public final class SafeBrowsingSettingsFragmentHelper {
    private SafeBrowsingSettingsFragmentHelper() {}

    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/40751023): Update when LaunchIntentDispatcher is (partially-)modularized.
     * TODO(crbug.com/40237779): Use HelpAndFeedbackLauncher after bug has been fixed.
     */
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }
}
