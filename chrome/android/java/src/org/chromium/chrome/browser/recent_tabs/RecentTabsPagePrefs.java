// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;

/** Allows Java code to read and modify preferences related to the {@link RecentTabsPage}. */
@NullMarked
class RecentTabsPagePrefs {
    @JniType("::RecentTabsPagePrefs")
    interface NativeRecentTabsPagePrefs extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeRecentTabsPagePrefs> mNativePrefs;

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
        assert mNativePrefs != null;
        mNativePrefs.destroy();
        mNativePrefs = null;
    }

    /**
     * Sets whether the list of snapshot documents is collapsed (vs expanded) on the Recent Tabs
     * page.
     * @param isCollapsed Whether we want the snapshot documents list to be collapsed.
     */
    void setSnapshotDocumentCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get()
                .setSnapshotDocumentCollapsed(assumeNonNull(mNativePrefs), isCollapsed);
    }

    /**
     * Gets whether the list of snapshot documents is collapsed (vs expanded) on
     * the Recent Tabs page.
     * @return Whether the list of snapshot documents is collapsed (vs expanded) on
     *         the Recent Tabs page.
     */
    boolean getSnapshotDocumentCollapsed() {
        return RecentTabsPagePrefsJni.get()
                .getSnapshotDocumentCollapsed(assumeNonNull(mNativePrefs));
    }

    /**
     * Sets whether the list of recently closed tabs is collapsed (vs expanded) on the Recent Tabs
     * page.
     * @param isCollapsed Whether we want the recently closed tabs list to be collapsed.
     */
    void setRecentlyClosedTabsCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get()
                .setRecentlyClosedTabsCollapsed(assumeNonNull(mNativePrefs), isCollapsed);
    }

    /**
     * Gets whether the list of recently closed tabs is collapsed (vs expanded) on
     * the Recent Tabs page.
     * @return Whether the list of recently closed tabs is collapsed (vs expanded) on
     *         the Recent Tabs page.
     */
    boolean getRecentlyClosedTabsCollapsed() {
        return RecentTabsPagePrefsJni.get()
                .getRecentlyClosedTabsCollapsed(assumeNonNull(mNativePrefs));
    }

    /**
     * Sets whether sync promo is collapsed (vs expanded) on the Recent Tabs page.
     * @param isCollapsed Whether we want the sync promo to be collapsed.
     */
    void setSyncPromoCollapsed(boolean isCollapsed) {
        RecentTabsPagePrefsJni.get()
                .setSyncPromoCollapsed(assumeNonNull(mNativePrefs), isCollapsed);
    }

    /**
     * Gets whether sync promo is collapsed (vs expanded) on the Recent Tabs page.
     * @return Whether the sync promo is collapsed (vs expanded) on the Recent Tabs page.
     */
    boolean getSyncPromoCollapsed() {
        return RecentTabsPagePrefsJni.get().getSyncPromoCollapsed(assumeNonNull(mNativePrefs));
    }

    /**
     * Sets whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     * @param session Session to set collapsed or expanded.
     * @param isCollapsed Whether we want the foreign session to be collapsed.
     */
    void setForeignSessionCollapsed(ForeignSession session, boolean isCollapsed) {
        RecentTabsPagePrefsJni.get()
                .setForeignSessionCollapsed(assumeNonNull(mNativePrefs), session.tag, isCollapsed);
    }

    /**
     * Gets whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     * @param  session Session to fetch collapsed state.
     * @return Whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     */
    boolean getForeignSessionCollapsed(ForeignSession session) {
        return RecentTabsPagePrefsJni.get()
                .getForeignSessionCollapsed(assumeNonNull(mNativePrefs), session.tag);
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeRecentTabsPagePrefs> init(@JniType("Profile*") Profile profile);

        void setSnapshotDocumentCollapsed(
                JniPtr<NativeRecentTabsPagePrefs> self, boolean isCollapsed);

        boolean getSnapshotDocumentCollapsed(JniPtr<NativeRecentTabsPagePrefs> self);

        void setRecentlyClosedTabsCollapsed(
                JniPtr<NativeRecentTabsPagePrefs> self, boolean isCollapsed);

        boolean getRecentlyClosedTabsCollapsed(JniPtr<NativeRecentTabsPagePrefs> self);

        void setSyncPromoCollapsed(JniPtr<NativeRecentTabsPagePrefs> self, boolean isCollapsed);

        boolean getSyncPromoCollapsed(JniPtr<NativeRecentTabsPagePrefs> self);

        void setForeignSessionCollapsed(
                JniPtr<NativeRecentTabsPagePrefs> self,
                @JniType("std::string") String sessionTag,
                boolean isCollapsed);

        boolean getForeignSessionCollapsed(
                JniPtr<NativeRecentTabsPagePrefs> self, @JniType("std::string") String sessionTag);
    }
}
