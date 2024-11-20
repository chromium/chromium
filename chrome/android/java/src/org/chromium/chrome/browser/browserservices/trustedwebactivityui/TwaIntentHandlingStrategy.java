// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.sharing.TwaSharingController;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;

/**
 * TWA-specific implementation of {@link CustomTabIntentHandlingStrategy}. Currently adds Web Share
 * Target capabilities on top of the Custom Tabs intent handling.
 */
public class TwaIntentHandlingStrategy implements CustomTabIntentHandlingStrategy {
    private final CustomTabIntentHandlingStrategy mDefaultStrategy;
    private final TwaSharingController mSharingController;

    public TwaIntentHandlingStrategy(
            CustomTabIntentHandlingStrategy defaultStrategy,
            TwaSharingController sharingController) {
        mDefaultStrategy = defaultStrategy;
        mSharingController = sharingController;
    }

    @Override
    public void handleInitialIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        handleIntent(intentDataProvider, /* isInitialIntent= */ true);
    }

    @Override
    public void handleNewIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        // TODO(pshmakov): we can have a significant delay here in case of POST sharing.
        // Allow showing splash screen, if it's provided in the intent.
        handleIntent(intentDataProvider, /* isInitialIntent= */ false);
    }

    private void handleIntent(
            BrowserServicesIntentDataProvider intentDataProvider, boolean isInitialIntent) {
        mSharingController
                .deliverToShareTarget(intentDataProvider)
                .then(
                        (delivered) -> {
                            if (delivered) return;

                            if (isInitialIntent) {
                                mDefaultStrategy.handleInitialIntent(intentDataProvider);
                            } else {
                                mDefaultStrategy.handleNewIntent(intentDataProvider);
                            }
                        });
    }
}
