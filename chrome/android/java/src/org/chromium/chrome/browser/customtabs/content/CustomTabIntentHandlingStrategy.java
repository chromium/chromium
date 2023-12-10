// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;

/** Strategy of handling incoming intents. */
public interface CustomTabIntentHandlingStrategy {

    /**
     * Called on start of activity after initialization routines (native init, creating tab)
     * have finished.
     *
     * @param intentDataProvider Provides the parameters sent with the initial intent.
     */
    void handleInitialIntent(BrowserServicesIntentDataProvider intentDataProvider);

    /**
     * Called when a valid new intent is delivered to the running Custom Tab. Initialization
     * routines are guaranteed to have been completed when this method is called.
     *
     * @param intentDataProvider Provides the parameters sent with the new intent.
     */
    void handleNewIntent(BrowserServicesIntentDataProvider intentDataProvider);
}
