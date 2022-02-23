// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.GoogleAPIKeys;
import org.chromium.chrome.browser.feed.FeedProcessScopeDependencyProvider;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;

/**
 * Creates a ProcessScopeDependencyProvider.
 *
 * Note that because ProcessScopeDependencyProvider is sometimes needed in response to a Native
 * call, we use this through reflection rather than simply injecting an instance.
 */
@UsedByReflection("FeedServiceBridge")
public class ProcessScopeDependencyProviderFactory {
    @UsedByReflection("FeedServiceBridge")
    public static FeedProcessScopeDependencyProvider create() {
        return new FeedProcessScopeDependencyProvider(
                GoogleAPIKeys.GOOGLE_API_KEY, PrivacyPreferencesManagerImpl.getInstance());
    }
}
