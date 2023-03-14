// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/**
 * {@link PageAnnotationService} cached by {@link Profile}.
 */
public class PageAnnotationsServiceFactory {
    @VisibleForTesting
    protected static ProfileKeyedMap<PageAnnotationsService> sProfileToPageAnnotationsService;

    /** Creates new instance. */
    public PageAnnotationsServiceFactory() {
        if (sProfileToPageAnnotationsService == null) {
            sProfileToPageAnnotationsService = new ProfileKeyedMap<PageAnnotationsService>(
                    ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
    }

    /**
     * Creates a new instance or reuses an existing one based on the current {@link Profile}.
     *
     * Note: Don't hold a reference to the returned value. Always use this method to access {@link
     * PageAnnotationService} instead.
     * @return {@link PageAnnotationsService} instance for the current regular
     *         profile.
     */
    public PageAnnotationsService getForLastUsedProfile() {
        Profile profile = Profile.getLastUsedRegularProfile();
        return sProfileToPageAnnotationsService.getForProfile(profile,
                () -> new PageAnnotationsService(new PageAnnotationsServiceProxy(profile)));
    }
}
