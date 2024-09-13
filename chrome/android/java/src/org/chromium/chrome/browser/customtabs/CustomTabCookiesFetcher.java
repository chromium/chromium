// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.ProfileProvider;

// TODO(crbug.com/360159401): Add a cleanup job for unused cookie files.
/**
 * Handles the Custom Tab specific behaviors of cookies persistence for off the record Custom tabs.
 */
public class CustomTabCookiesFetcher extends CookiesFetcher {
    private static final String COOKIE_FILE_PREFIX = "COOKIES_";
    private static final String COOKIE_FILE_EXTENSION = ".DAT";

    private final int mTaskId;

    /**
     * @param taskId The task ID that the owning Custom Tab is in.
     * @see {@link CookiesFetcher}.
     */
    public CustomTabCookiesFetcher(
            ProfileProvider profileProvider, CipherFactory cipherFactory, int taskId) {
        super(profileProvider, cipherFactory);
        mTaskId = taskId;
    }

    @Override
    protected String fetchFileName() {
        return COOKIE_FILE_PREFIX + mTaskId + COOKIE_FILE_EXTENSION;
    }

    @Override
    protected boolean isLegacyFileApplicable() {
        // Legacy cookie file was never used for Custom tab activities.
        return false;
    }
}
