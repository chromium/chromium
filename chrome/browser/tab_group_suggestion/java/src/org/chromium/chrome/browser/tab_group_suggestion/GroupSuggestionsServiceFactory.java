// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;

/** This factory creates GroupSuggestionsService for the given {@link Profile}. */
public final class GroupSuggestionsServiceFactory {

    // Don't instantiate me.
    private GroupSuggestionsServiceFactory() {}

    /**
     * A factory method to create or retrieve a {@link GroupSuggestionsService} object for a given
     * profile.
     *
     * @return The {@link GroupSuggestionsService} for the given profile.
     */
    public static GroupSuggestionsService getForProfile(Profile profile) {
        return GroupSuggestionsServiceFactoryJni.get().getForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        GroupSuggestionsService getForProfile(@JniType("Profile*") Profile profile);
    }
}
