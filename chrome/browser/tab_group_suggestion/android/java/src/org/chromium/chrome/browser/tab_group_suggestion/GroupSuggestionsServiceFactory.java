// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;

/** This factory creates GroupSuggestionsService for the given {@link Profile}. */
@NullMarked
public final class GroupSuggestionsServiceFactory {
    private static @Nullable GroupSuggestionsService sGroupSuggestionsServiceForTesting;

    // Don't instantiate me.
    private GroupSuggestionsServiceFactory() {}

    /**
     * A factory method to create or retrieve a {@link GroupSuggestionsService} object for a given
     * profile.
     *
     * @return The {@link GroupSuggestionsService} for the given profile.
     */
    public static GroupSuggestionsService getForProfile(Profile profile) {
        if (sGroupSuggestionsServiceForTesting != null) {
            return sGroupSuggestionsServiceForTesting;
        }
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.GROUP_SUGGESTION_SERVICE)) {
            throw new IllegalStateException("GroupSuggestionsService is not enabled.");
        }
        if (profile.isOffTheRecord()) {
            throw new IllegalStateException(
                    "GroupSuggestionsService is not supported in incognito.");
        }
        return GroupSuggestionsServiceFactoryJni.get().getForProfile(profile);
    }

    public static void setGroupSuggestionsServiceForTesting(
            GroupSuggestionsService groupSuggestionsService) {
        sGroupSuggestionsServiceForTesting = groupSuggestionsService;
        ResettersForTesting.register(() -> sGroupSuggestionsServiceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        GroupSuggestionsService getForProfile(@JniType("Profile*") Profile profile);
    }
}
