// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion.toolbar;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsServiceFactory;

/** This factory creates and keeps a single GroupSuggestionsButtonController per profile. */
@NullMarked
public class GroupSuggestionsButtonControllerFactory {

    private static final ProfileKeyedMap<GroupSuggestionsButtonController> sProfileMap =
            new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
    private static @Nullable GroupSuggestionsButtonController sButtonControllerForTesting;

    private GroupSuggestionsButtonControllerFactory() {}

    @Nullable
    public static GroupSuggestionsButtonController getForProfile(Profile profile) {
        if (sButtonControllerForTesting != null) {
            return sButtonControllerForTesting;
        }

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.GROUP_SUGGESTION_SERVICE)) {
            return null;
        }

        return sProfileMap.getForProfile(
                profile, GroupSuggestionsButtonControllerFactory::buildForProfile);
    }

    public static void setControllerForTesting(GroupSuggestionsButtonController controller) {
        sButtonControllerForTesting = controller;
        ResettersForTesting.register(() -> sButtonControllerForTesting = null);
    }

    private static GroupSuggestionsButtonController buildForProfile(Profile profile) {
        var groupSuggestionsService = GroupSuggestionsServiceFactory.getForProfile(profile);
        return new GroupSuggestionsButtonControllerImpl(groupSuggestionsService);
    }
}
