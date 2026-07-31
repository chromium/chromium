// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/** Data model representing a background session (either provisioned or transitioned). */
// TODO(crbug.com/540441211): Handle multi-window reparenting case.
@NullMarked
public class BackgroundSession {

    private final List<Tab> mTabs = new ArrayList<>();
    private final @Nullable String mGlicTriggerMessageId;

    private @Nullable Integer mTaskId;

    /**
     * Constructor for a session transitioned from an active Chrome tab.
     *
     * @param tab The tab being transitioned.
     * @param taskId The ID of the task associated with the tab.
     */
    public BackgroundSession(Tab tab, int taskId) {
        mTabs.add(tab);
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
        mTabs.add(tab);
        mTaskId = null;
        mGlicTriggerMessageId = glicTriggerMessageId;
    }

    /** Adds an additional tab associated with this session. */
    public void addTab(Tab tab) {
        mTabs.add(tab);
    }

    /** Returns all offscreen tabs associated with this session. */
    public List<Tab> getTabs() {
        return mTabs;
    }

    /** Returns the last active offscreen tab owned by this session. */
    public Tab getLastActiveTab() {
        return ActorTaskHelper.getLastActiveTabForTask(mTabs, mTaskId);
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
}
