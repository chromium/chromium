// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/** Data model representing a background session (either provisioned or transitioned). */
// TODO(crbug.com/540441211): Handle multi-window reparenting case.
@NullMarked
public class BackgroundSession {

    /** Inner class to house per-tab data. */
    public static class BackgroundTabData {
        private final Tab mTab;
        private @Nullable @TabId Integer mPlaceholderTabId;
        private int mOriginalTabIndex = TabModel.INVALID_TAB_INDEX;
        private int mTabWindowId = -1;

        public BackgroundTabData(Tab tab) {
            mTab = tab;
        }

        public BackgroundTabData(
                Tab tab, @Nullable @TabId Integer placeholderTabId, int originalTabIndex) {
            mTab = tab;
            mPlaceholderTabId = placeholderTabId;
            mOriginalTabIndex = originalTabIndex;
        }

        public BackgroundTabData(
                Tab tab,
                @Nullable @TabId Integer placeholderTabId,
                int originalTabIndex,
                int tabWindowId) {
            mTab = tab;
            mPlaceholderTabId = placeholderTabId;
            mOriginalTabIndex = originalTabIndex;
            mTabWindowId = tabWindowId;
        }

        /** Returns the tab associated with this data. */
        public Tab getTab() {
            return mTab;
        }

        /** Returns the ID of the placeholder tab associated with this tab, or null. */
        public @Nullable @TabId Integer getPlaceholderTabId() {
            return mPlaceholderTabId;
        }

        /** Sets the ID of the placeholder tab associated with this tab. */
        public void setPlaceholderTabId(@TabId int placeholderTabId) {
            mPlaceholderTabId = placeholderTabId;
        }

        /** Returns the original index of the tab in the tab model before transition. */
        public int getOriginalTabIndex() {
            return mOriginalTabIndex;
        }

        /** Sets the original index of the tab in the tab model before transition. */
        public void setOriginalTabIndex(int originalTabIndex) {
            mOriginalTabIndex = originalTabIndex;
        }

        /** Returns the window ID associated with this tab. */
        public int getTabWindowId() {
            return mTabWindowId;
        }

        /** Sets the window ID associated with this tab. */
        public void setTabWindowId(int tabWindowId) {
            mTabWindowId = tabWindowId;
        }
    }

    private final List<BackgroundTabData> mTabDataList = new ArrayList<>();
    private final @Nullable String mGlicTriggerMessageId;

    private @Nullable Integer mTaskId;

    /**
     * Constructor for a session transitioned from an active Chrome tab.
     *
     * @param tab The tab being transitioned.
     * @param taskId The ID of the task associated with the tab.
     */
    public BackgroundSession(Tab tab, int taskId) {
        mTabDataList.add(new BackgroundTabData(tab));
        mTaskId = taskId;
        mGlicTriggerMessageId = null;
    }

    /**
     * Constructor for a session transitioned from an active Chrome tab.
     *
     * @param tabData The data of the tab being transitioned.
     * @param taskId The ID of the task associated with the tab.
     */
    public BackgroundSession(BackgroundTabData tabData, int taskId) {
        mTabDataList.add(tabData);
        mTaskId = taskId;
        mGlicTriggerMessageId = null;
    }

    /**
     * Constructor for a session started in the background via a trigger message.
     *
     * @param tab The newly provisioned tab.
     * @param glicTriggerMessageId The ID of the triggering message.
     */
    public BackgroundSession(Tab tab, String glicTriggerMessageId) {
        mTabDataList.add(new BackgroundTabData(tab));
        mTaskId = null;
        mGlicTriggerMessageId = glicTriggerMessageId;
    }

    /** Adds an additional tab associated with this session. */
    public void addTab(Tab tab) {
        mTabDataList.add(new BackgroundTabData(tab));
    }

    /**
     * Adds an existing tab data object to this session.
     *
     * @param tabData The tab data being added.
     */
    public void addTabData(BackgroundTabData tabData) {
        mTabDataList.add(tabData);
    }

    /** Returns all offscreen tabs associated with this session. */
    public List<Tab> getTabs() {
        List<Tab> tabs = new ArrayList<>(mTabDataList.size());
        for (BackgroundTabData data : mTabDataList) {
            tabs.add(data.getTab());
        }
        return tabs;
    }

    /** Returns data for all tabs associated with this session. */
    public List<BackgroundTabData> getTabDataList() {
        return mTabDataList;
    }

    /** Returns the last active offscreen tab owned by this session. */
    public Tab getLastActiveTab() {
        return ActorTaskHelper.getLastActiveTabForTask(getTabs(), mTaskId);
    }

    /** Returns the task ID associated with this session, or null if not yet assigned. */
    public @Nullable Integer getTaskId() {
        return mTaskId;
    }

    /** Sets the task ID associated with this session. */
    public void setTaskId(int taskId) {
        assert mTaskId == null;
        mTaskId = taskId;
    }

    /** Returns the triggering message ID associated with this session, or null. */
    public @Nullable String getGlicTriggerMessageId() {
        return mGlicTriggerMessageId;
    }

    /** Finds a session with matching task ID from a list of sessions. */
    public static @Nullable BackgroundSession getSessionForTask(
            List<BackgroundSession> sessions, int taskId) {
        for (BackgroundSession session : sessions) {
            if (session.getTaskId() != null && session.getTaskId() == taskId) {
                return session;
            }
        }
        return null;
    }
}
