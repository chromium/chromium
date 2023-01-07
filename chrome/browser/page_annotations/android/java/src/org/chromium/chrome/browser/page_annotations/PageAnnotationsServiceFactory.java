// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.HashMap;
import java.util.Map;

/**
 * {@link PageAnnotationService} cached by {@link Profile}.
 */
public class PageAnnotationsServiceFactory {
    @VisibleForTesting
    protected static final Map<Profile, PageAnnotationsService> sProfileToPageAnnotationsService =
            new HashMap<>();
    private static ProfileManager.Observer sProfileManagerObserver;

    /** Creates new instance. */
    public PageAnnotationsServiceFactory() {
        if (sProfileManagerObserver == null) {
            sProfileManagerObserver = new ProfileManager.Observer() {
                @Override
                public void onProfileAdded(Profile profile) {}

                @Override
                public void onProfileDestroyed(Profile destroyedProfile) {
                    PageAnnotationsService serviceToDestroy =
                            sProfileToPageAnnotationsService.get(destroyedProfile);
                    if (serviceToDestroy != null) {
                        sProfileToPageAnnotationsService.remove(destroyedProfile);
                    }

                    if (sProfileToPageAnnotationsService.isEmpty()) {
                        ProfileManager.removeObserver(sProfileManagerObserver);
                        sProfileManagerObserver = null;
                    }
                }
            };
            ProfileManager.addObserver(sProfileManagerObserver);
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
        PageAnnotationsService service = sProfileToPageAnnotationsService.get(profile);
        if (service == null) {
            service = new PageAnnotationsService(new PageAnnotationsServiceProxy(profile));
            sProfileToPageAnnotationsService.put(profile, service);
        }
        return service;
    }
}
