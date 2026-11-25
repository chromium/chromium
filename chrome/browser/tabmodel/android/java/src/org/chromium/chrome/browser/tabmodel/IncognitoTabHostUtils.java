// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;

/** Utilities concerning all incognito tabs in all {@link IncognitoTabHost}s. */
@NullMarked
public class IncognitoTabHostUtils {
    /**
     * Returns whether {@code profileProvider}'s off-the-record profile matches {@code otrProfile}.
     */
    public static boolean matchesProfile(
            @Nullable ProfileProvider profileProvider, Profile otrProfile) {
        assert otrProfile.isOffTheRecord();
        return profileProvider != null
                && otrProfile.equals(profileProvider.getOffTheRecordProfile());
    }

    /** Determines whether there are any incognito tabs across all profiles. */
    public static boolean doIncognitoTabsExist() {
        return doIncognitoTabsExistInternal(/* otrProfile= */ null);
    }

    /**
     * Determines whether there are any incognito tabs for the given off-the-record {@link Profile}.
     */
    public static boolean doIncognitoTabsExistForProfile(Profile otrProfile) {
        assert otrProfile.isOffTheRecord();
        return doIncognitoTabsExistInternal(otrProfile);
    }

    private static boolean doIncognitoTabsExistInternal(@Nullable Profile otrProfile) {
        for (IncognitoTabHost host : IncognitoTabHostRegistry.getInstance().getHosts()) {
            if (host.hasIncognitoTabs(otrProfile)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Determines whether an incognito tab model is active for the given off-the-record {@link
     * Profile}.
     */
    public static boolean isIncognitoTabModelActive(Profile otrProfile) {
        assert otrProfile.isOffTheRecord();
        for (IncognitoTabHost host : IncognitoTabHostRegistry.getInstance().getHosts()) {
            if (host.isActiveModel(otrProfile)) {
                return true;
            }
        }
        return false;
    }

    /** Closes all incognito tabs across all profiles. */
    public static void closeAllIncognitoTabs() {
        for (IncognitoTabHost host : IncognitoTabHostRegistry.getInstance().getHosts()) {
            host.closeAllIncognitoTabs();
        }
    }
}
