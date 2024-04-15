// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;

/** Allows Java code to read and modify preferences related to the {@link RecentTabsPage}. */
class RecentTabsPagePrefs {
    private long mNativePrefs;

    /**
     * Initialize this class with the given profile.
     * @param profile Profile that will be used for syncing.
     */
    RecentTabsPagePrefs(Profile profile) {
        mNativePrefs = RecentTabsPagePrefsJni.get().init(profile);
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    void destroy() {
        assert mNativePrefs != 0;
        RecentTabsPagePrefsJni.get().destroy(mNativePrefs);
        mNativePrefs = 0;
    }

    /**
     * Sets whether the list of snapshot documents is collapsed (vs expanded) on the Recent Tabs
     * page.
     * @param isCollapsed Whether we want the snapshot documents list to be collapsed.
     */
    void setSnapshotDocumentCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get().setSnapshotDocumentCollapsed(mNativePrefs, isCollapsed);
    }

    /**
     * Gets whether the list of snapshot documents is collapsed (vs expanded) on
     * the Recent Tabs page.
     * @return Whether the list of snapshot documents is collapsed (vs expanded) on
     *         the Recent Tabs page.
     */
    boolean getSnapshotDocumentCollapsed() {
        return RecentTabsPagePrefsJni.get().getSnapshotDocumentCollapsed(mNativePrefs);
    }

    /**
     * Sets whether the list of recently closed tabs is collapsed (vs expanded) on the Recent Tabs
     * page.
     * @param isCollapsed Whether we want the recently closed tabs list to be collapsed.
     */
    void setRecentlyClosedTabsCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get().setRecentlyClosedTabsCollapsed(mNativePrefs, isCollapsed);
    }

    /**
     * Gets whether the list of recently closed tabs is collapsed (vs expanded) on
     * the Recent Tabs page.
     * @return Whether the list of recently closed tabs is collapsed (vs expanded) on
     *         the Recent Tabs page.
     */
    boolean getRecentlyClosedTabsCollapsed() {
        return RecentTabsPagePrefsJni.get().getRecentlyClosedTabsCollapsed(mNativePrefs);
    }

    /**
     * Sets whether sync promo is collapsed (vs expanded) on the Recent Tabs page.
     * @param isCollapsed Whether we want the sync promo to be collapsed.
     */
    void setSyncPromoCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get().setSyncPromoCollapsed(mNativePrefs, isCollapsed);
    }

    /**
     * Gets whether sync promo is collapsed (vs expanded) on the Recent Tabs page.
     * @return Whether the sync promo is collapsed (vs expanded) on the Recent Tabs page.
     */
    boolean getSyncPromoCollapsed() {
        return RecentTabsPagePrefsJni.get().getSyncPromoCollapsed(mNativePrefs);
    }

    /**
     * Sets whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     * @param session Session to set collapsed or expanded.
     * @param isCollapsed Whether we want the foreign session to be collapsed.
     */
    void setForeignSessionCollapsed(ForeignSession session, boolean isCollapsed) {
        RecentTabsPagePrefsJni.get()
                .setForeignSessionCollapsed(mNativePrefs, session.tag, isCollapsed);
    }

    /**
     * Gets whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     * @param  session Session to fetch collapsed state.
     * @return Whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     */
    boolean getForeignSessionCollapsed(ForeignSession session) {
        return RecentTabsPagePrefsJni.get().getForeignSessionCollapsed(mNativePrefs, session.tag);
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void destroy(long nativeRecentTabsPagePrefs);

        void setSnapshotDocumentCollapsed(long nativeRecentTabsPagePrefs, boolean isCollapsed);

        boolean getSnapshotDocumentCollapsed(long nativeRecentTabsPagePrefs);

        void setRecentlyClosedTabsCollapsed(long nativeRecentTabsPagePrefs, boolean isCollapsed);

        boolean getRecentlyClosedTabsCollapsed(long nativeRecentTabsPagePrefs);

        void setSyncPromoCollapsed(long nativeRecentTabsPagePrefs, boolean isCollapsed);

        boolean getSyncPromoCollapsed(long nativeRecentTabsPagePrefs);

        void setForeignSessionCollapsed(
                long nativeRecentTabsPagePrefs, String sessionTag, boolean isCollapsed);

        boolean getForeignSessionCollapsed(long nativeRecentTabsPagePrefs, String sessionTag);
    }
}
