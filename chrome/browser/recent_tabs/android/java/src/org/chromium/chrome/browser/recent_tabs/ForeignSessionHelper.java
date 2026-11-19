// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * This class exposes to Java information about sessions, windows, and tabs on the user's synced
 * devices.
 */
@NullMarked
public class ForeignSessionHelper {
    @JniType("::ForeignSessionHelper")
    interface NativeForeignSessionHelper extends JniTypeToken {}

    private final JniUniquePtr<NativeForeignSessionHelper> mNativeForeignSessionHelper;

    /** Callback interface for getting notified when foreign session sync is updated. */
    public interface ForeignSessionCallback {
        /**
         * This method will be called every time foreign session sync is updated.
         *
         * It's a good place to call {@link ForeignSessionHelper#getForeignSessions()} to get the
         * updated information.
         */
        @CalledByNative
        void onUpdated();
    }

    /**
     * Represents synced foreign session.
     * Form factor value correlations can be found in the components/sync_device_info/device_info.h
     * file or its Java generated counterpart.
     */
    public static class ForeignSession {
        public final String tag;
        public final String name;
        public final long modifiedTime;
        public final List<ForeignSessionWindow> windows = new ArrayList<>();
        public final @FormFactor int formFactor;

        private ForeignSession(
                String tag, String name, long modifiedTime, @FormFactor int formFactor) {
            this(tag, name, modifiedTime, new ArrayList<>(), formFactor);
        }

        @VisibleForTesting
        public ForeignSession(
                String tag,
                String name,
                long modifiedTime,
                List<ForeignSessionWindow> windows,
                @FormFactor int formFactor) {
            this.tag = tag;
            this.name = name;
            this.modifiedTime = modifiedTime;
            this.windows.addAll(windows);
            this.formFactor = formFactor;
        }
    }

    /**
     * Represents synced foreign window. Note that desktop Chrome can have multiple windows in a
     * session.
     */
    public static class ForeignSessionWindow {
        public final long timestamp;
        public final int sessionId;
        public final List<ForeignSessionTab> tabs = new ArrayList<>();

        private ForeignSessionWindow(long timestamp, int sessionId) {
            this(timestamp, sessionId, new ArrayList<>());
        }

        @VisibleForTesting
        public ForeignSessionWindow(long timestamp, int sessionId, List<ForeignSessionTab> tabs) {
            this.timestamp = timestamp;
            this.sessionId = sessionId;
            this.tabs.addAll(tabs);
        }
    }

    /** Represents synced foreign tab. */
    public static class ForeignSessionTab {
        public final GURL url;
        public final String title;
        public final long timestamp;
        public final long lastActiveTime;
        public final int id;
        public final boolean pinned;

        @VisibleForTesting
        public ForeignSessionTab(
                GURL url, String title, long timestamp, long lastActiveTime, int id) {
            this(url, title, timestamp, lastActiveTime, id, /* pinned= */ false);
        }

        @VisibleForTesting
        public ForeignSessionTab(
                GURL url,
                String title,
                long timestamp,
                long lastActiveTime,
                int id,
                boolean pinned) {
            this.url = url;
            this.title = title;
            this.timestamp = timestamp;
            this.lastActiveTime = lastActiveTime;
            this.id = id;
            this.pinned = pinned;
        }
    }

    @CalledByNative
    private static ForeignSession pushSession(
            List<ForeignSession> sessions,
            @JniType("std::string") String tag,
            @JniType("std::string") String name,
            long modifiedTime,
            @FormFactor int formFactor) {
        ForeignSession session = new ForeignSession(tag, name, modifiedTime, formFactor);
        sessions.add(session);
        return session;
    }

    @CalledByNative
    private static ForeignSessionWindow pushWindow(
            ForeignSession session, long timestamp, int sessionId) {
        ForeignSessionWindow window = new ForeignSessionWindow(timestamp, sessionId);
        session.windows.add(window);
        return window;
    }

    @CalledByNative
    private static void pushTab(
            ForeignSessionWindow window,
            @JniType("GURL") GURL url,
            @JniType("std::u16string") String title,
            long timestamp,
            long lastActiveTime,
            int sessionId,
            boolean pinned) {
        ForeignSessionTab tab =
                new ForeignSessionTab(url, title, timestamp, lastActiveTime, sessionId, pinned);
        window.tabs.add(tab);
    }

    /**
     * Initialize this class with the given profile.
     * @param profile Profile that will be used for syncing.
     */
    public ForeignSessionHelper(Profile profile) {
        mNativeForeignSessionHelper = ForeignSessionHelperJni.get().init(profile);
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        mNativeForeignSessionHelper.destroy();
    }

    /**
     * @return {@code True} iff Tab sync is enabled.
     */
    public boolean isTabSyncEnabled() {
        return ForeignSessionHelperJni.get().isTabSyncEnabled(mNativeForeignSessionHelper);
    }

    /** Force a sync for sessions. */
    public void triggerSessionSync() {
        ForeignSessionHelperJni.get().triggerSessionSync(mNativeForeignSessionHelper);
    }

    /**
     * Sets callback instance that will be called on every foreign session sync update.
     * @param callback The callback to be invoked.
     */
    public void setOnForeignSessionCallback(ForeignSessionCallback callback) {
        ForeignSessionHelperJni.get()
                .setOnForeignSessionCallback(mNativeForeignSessionHelper, callback);
    }

    /**
     * @return The list of synced foreign sessions. If it fails to get them for some reason will
     *     return an empty list.
     */
    public List<ForeignSession> getForeignSessions() {
        List<ForeignSession> result = new ArrayList<>();
        boolean received =
                ForeignSessionHelperJni.get()
                        .getForeignSessions(mNativeForeignSessionHelper, result);
        if (!received) {
            result = Collections.emptyList();
        }

        return result;
    }

    /**
     * @return The list of synced foreign sessions that are from a mobile or tablet device. If it
     *     fails to get them for some reason will return an empty list.
     */
    public List<ForeignSession> getMobileAndTabletForeignSessions() {
        List<ForeignSession> result = new ArrayList<>();
        boolean received =
                ForeignSessionHelperJni.get()
                        .getMobileAndTabletForeignSessions(mNativeForeignSessionHelper, result);
        if (!received) {
            result = Collections.emptyList();
        }

        return result;
    }

    /**
     * Opens the given foreign tab in a new tab.
     *
     * @param tab Tab to load the session into.
     * @param session Session that the target tab belongs to.
     * @param foreignTab Target tab to open.
     * @param windowOpenDisposition The WindowOpenDisposition flag.
     * @return {@code True} iff the tab is successfully opened.
     */
    public boolean openForeignSessionTab(
            Tab tab,
            ForeignSession session,
            ForeignSessionTab foreignTab,
            int windowOpenDisposition) {
        return openForeignSessionTab(tab, session.tag, foreignTab.id, windowOpenDisposition);
    }

    /**
     * Opens the given foreign tab in a new tab.
     *
     * @param tab Tab to load the session into.
     * @param sessionTag Tag of the session that the target tab belongs to.
     * @param tabId ID of the target tab to open.
     * @param windowOpenDisposition The WindowOpenDisposition flag.
     * @return {@code True} iff the tab is successfully opened.
     */
    public boolean openForeignSessionTab(
            Tab tab, String sessionTag, int tabId, int windowOpenDisposition) {
        return ForeignSessionHelperJni.get()
                .openForeignSessionTab(
                        mNativeForeignSessionHelper, tab, sessionTag, tabId, windowOpenDisposition);
    }

    /**
     * Remove Foreign session to display. Note that it will be reappear on the next sync.
     *
     * <p>This is mainly for when user wants to delete very old session that won't be used or synced
     * in the future.
     *
     * @param session Session to be deleted.
     */
    public void deleteForeignSession(ForeignSession session) {
        ForeignSessionHelperJni.get()
                .deleteForeignSession(mNativeForeignSessionHelper, session.tag);
    }

    /** Enable invalidations for sessions sync related datatypes. */
    public void setInvalidationsForSessionsEnabled(boolean enabled) {
        ForeignSessionHelperJni.get()
                .setInvalidationsForSessionsEnabled(mNativeForeignSessionHelper, enabled);
    }

    /**
     * Lazily instantiate a list of tabs as background tabs.
     *
     * @param sessionTabs List of target tabs to open.
     * @param session Foreign session that holds the list of target tabs to open.
     * @param tabCreatorManager Tab creator manager to create a new foreground tab for tab restore.
     * @return One entry per tab in {@code sessionTabs}, in the same order: the ID of the restored
     *     local tab, or {@link Tab#INVALID_TAB_ID} if that tab was not restored.
     */
    public @TabId int[] openForeignSessionTabsAsBackgroundTabs(
            List<ForeignSessionTab> sessionTabs,
            ForeignSession session,
            TabCreatorManager tabCreatorManager) {
        @TabId int[] restoredTabIds = new int[sessionTabs.size()];
        Arrays.fill(restoredTabIds, Tab.INVALID_TAB_ID);
        if (sessionTabs.isEmpty()) {
            return restoredTabIds;
        }
        Tab newForegroundTab =
                tabCreatorManager
                        .getTabCreator(/* incognito= */ false)
                        .createNewTab(
                                new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_URL),
                                TabLaunchType.FROM_RESTORE_TABS_UI,
                                /* parent= */ null);
        if (newForegroundTab == null) return restoredTabIds;

        return restoreTabs(sessionTabs, session, newForegroundTab, /* allInBackground= */ false);
    }

    /**
     * Restores a list of tabs as background tabs without a renderer and without changing the
     * selected tab.
     *
     * @param sessionTabs List of target tabs to open.
     * @param session Foreign session that holds the list of target tabs to open.
     * @param anchorTab Existing tab whose tab model receives the restored tabs. The restored tabs
     *     use it as their parent. It stays open; the caller owns it.
     * @return One entry per tab in {@code sessionTabs}, in the same order: the ID of the restored
     *     local tab, or {@link Tab#INVALID_TAB_ID} if that tab was not restored.
     */
    public @TabId int[] openForeignSessionTabsInBackground(
            List<ForeignSessionTab> sessionTabs, ForeignSession session, Tab anchorTab) {
        return restoreTabs(sessionTabs, session, anchorTab, /* allInBackground= */ true);
    }

    private @TabId int[] restoreTabs(
            List<ForeignSessionTab> sessionTabs,
            ForeignSession session,
            Tab anchorTab,
            boolean allInBackground) {
        int[] sessionTabIds = new int[sessionTabs.size()];
        for (int i = 0; i < sessionTabs.size(); i++) {
            sessionTabIds[i] = sessionTabs.get(i).id;
        }

        @TabId
        int[] restoredTabIds =
                ForeignSessionHelperJni.get()
                        .openForeignSessionTabsAsBackgroundTabs(
                                mNativeForeignSessionHelper,
                                anchorTab,
                                sessionTabIds,
                                session.tag,
                                allInBackground);
        for (@TabId int tabId : restoredTabIds) {
            if (tabId != Tab.INVALID_TAB_ID) {
                RecordUserAction.record("MobileCrossDeviceTabJourney");
            }
        }
        return restoredTabIds;
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeForeignSessionHelper> init(@JniType("Profile*") Profile profile);

        boolean isTabSyncEnabled(JniPtr<NativeForeignSessionHelper> self);

        void triggerSessionSync(JniPtr<NativeForeignSessionHelper> self);

        void setOnForeignSessionCallback(
                JniPtr<NativeForeignSessionHelper> self, ForeignSessionCallback callback);

        boolean getForeignSessions(
                JniPtr<NativeForeignSessionHelper> self, List<ForeignSession> resultSessions);

        boolean getMobileAndTabletForeignSessions(
                JniPtr<NativeForeignSessionHelper> self, List<ForeignSession> resultSessions);

        boolean openForeignSessionTab(
                JniPtr<NativeForeignSessionHelper> self,
                @JniType("TabAndroid*") Tab tab,
                @JniType("std::string") String sessionTag,
                int tabId,
                int disposition);

        void deleteForeignSession(
                JniPtr<NativeForeignSessionHelper> self, @JniType("std::string") String sessionTag);

        void setInvalidationsForSessionsEnabled(
                JniPtr<NativeForeignSessionHelper> self, boolean enabled);

        @JniType("std::vector<int32_t>")
        int[] openForeignSessionTabsAsBackgroundTabs(
                JniPtr<NativeForeignSessionHelper> self,
                @JniType("TabAndroid*") Tab tab,
                @JniType("std::vector<int32_t>") int[] tabIds,
                @JniType("std::string") String sessionTag,
                boolean allInBackground);
    }
}
