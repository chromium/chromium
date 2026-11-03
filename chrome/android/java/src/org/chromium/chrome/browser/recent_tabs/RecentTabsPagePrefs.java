// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;

/** Allows Java code to read and modify preferences related to the {@link RecentTabsPage}. */
@NullMarked
class RecentTabsPagePrefs {
    private final Profile mProfile;

    /**
     * Initialize this class with the given profile.
     *
     * @param profile Profile that will be used for syncing.
     */
    RecentTabsPagePrefs(Profile profile) {
        mProfile = profile;
    }

    /**
     * Sets whether the list of snapshot documents is collapsed (vs expanded) on the Recent Tabs
     * page.
     *
     * @param isCollapsed Whether we want the snapshot documents list to be collapsed.
     */
    void setSnapshotDocumentCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get().setSnapshotDocumentCollapsed(mProfile, isCollapsed);
    }

    /**
     * Gets whether the list of snapshot documents is collapsed (vs expanded) on the Recent Tabs
     * page.
     *
     * @return Whether the list of snapshot documents is collapsed (vs expanded) on the Recent Tabs
     *     page.
     */
    boolean getSnapshotDocumentCollapsed() {
        return RecentTabsPagePrefsJni.get().getSnapshotDocumentCollapsed(mProfile);
    }

    /**
     * Sets whether the list of recently closed tabs is collapsed (vs expanded) on the Recent Tabs
     * page.
     *
     * @param isCollapsed Whether we want the recently closed tabs list to be collapsed.
     */
    void setRecentlyClosedTabsCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get().setRecentlyClosedTabsCollapsed(mProfile, isCollapsed);
    }

    /**
     * Gets whether the list of recently closed tabs is collapsed (vs expanded) on the Recent Tabs
     * page.
     *
     * @return Whether the list of recently closed tabs is collapsed (vs expanded) on the Recent
     *     Tabs page.
     */
    boolean getRecentlyClosedTabsCollapsed() {
        return RecentTabsPagePrefsJni.get().getRecentlyClosedTabsCollapsed(mProfile);
    }

    /**
     * Sets whether sync promo is collapsed (vs expanded) on the Recent Tabs page.
     *
     * @param isCollapsed Whether we want the sync promo to be collapsed.
     */
    void setSyncPromoCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get().setSyncPromoCollapsed(mProfile, isCollapsed);
    }

    /**
     * Gets whether sync promo is collapsed (vs expanded) on the Recent Tabs page.
     *
     * @return Whether the sync promo is collapsed (vs expanded) on the Recent Tabs page.
     */
    boolean getSyncPromoCollapsed() {
        return RecentTabsPagePrefsJni.get().getSyncPromoCollapsed(mProfile);
    }

    /**
     * Sets whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     *
     * @param session Session to set collapsed or expanded.
     * @param isCollapsed Whether we want the foreign session to be collapsed.
     */
    void setForeignSessionCollapsed(ForeignSession session, boolean isCollapsed) {
        RecentTabsPagePrefsJni.get().setForeignSessionCollapsed(mProfile, session.tag, isCollapsed);
    }

    /**
     * Gets whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     *
     * @param session Session to fetch collapsed state.
     * @return Whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     */
    boolean getForeignSessionCollapsed(ForeignSession session) {
        return RecentTabsPagePrefsJni.get().getForeignSessionCollapsed(mProfile, session.tag);
    }

    @NativeMethods
    interface Natives {
        void setSnapshotDocumentCollapsed(
                @JniType("Profile*") Profile profile, boolean isCollapsed);

        boolean getSnapshotDocumentCollapsed(@JniType("Profile*") Profile profile);

        void setRecentlyClosedTabsCollapsed(
                @JniType("Profile*") Profile profile, boolean isCollapsed);

        boolean getRecentlyClosedTabsCollapsed(@JniType("Profile*") Profile profile);

        void setSyncPromoCollapsed(@JniType("Profile*") Profile profile, boolean isCollapsed);

        boolean getSyncPromoCollapsed(@JniType("Profile*") Profile profile);

        void setForeignSessionCollapsed(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String sessionTag,
                boolean isCollapsed);

        boolean getForeignSessionCollapsed(
                @JniType("Profile*") Profile profile, @JniType("std::string") String sessionTag);
    }
}
