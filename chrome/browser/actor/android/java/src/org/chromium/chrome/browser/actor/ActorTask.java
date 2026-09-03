// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;

import java.lang.ref.WeakReference;
import java.util.HashSet;
import java.util.Set;

/** Represents an ongoing actor interaction. This class is a wrapper around the native ActorTask. */
@JNINamespace("actor")
@NullMarked
public class ActorTask {
    public static final int INVALID_TASK_ID = -1;

    private long mNativeTask;
    private final int mId;
    private final String mTitle;
    private final WeakReference<Profile> mProfile;

    @CalledByNative
    private ActorTask(
            long nativeTask, int id, @JniType("std::string") String title, Profile profile) {
        mNativeTask = nativeTask;
        mId = id;
        mTitle = title;
        mProfile = new WeakReference<>(profile);
    }

    /**
     * @return The unique ID of the task.
     */
    public int getId() {
        return mId;
    }

    /**
     * @return The user-visible title of the task.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @return The name of the current action being executed.
     */
    public String getCurrentActionName() {
        if (mNativeTask == 0) return "";
        return ActorTaskJni.get().getCurrentActionName(mNativeTask);
    }

    /**
     * @return Current ActorTask::State.
     */
    public @ActorTaskState int getState() {
        if (mNativeTask == 0) return ActorTaskState.CREATED;
        return ActorTaskJni.get().getState(mNativeTask);
    }

    public boolean isCompleted() {
        if (mNativeTask == 0) return true;
        return ActorTaskJni.get().isCompleted(mNativeTask);
    }

    /** True if the actor is currently performing actions. */
    public boolean isUnderActorControl() {
        if (mNativeTask == 0) return false;
        return ActorTaskJni.get().isUnderActorControl(mNativeTask);
    }

    /** Triggers ActorTask::Pause(from_actor=false). */
    public void pause() {
        if (mNativeTask == 0) return;
        ActorTaskJni.get().pause(mNativeTask);
    }

    /** Triggers ActorTask::Resume(). */
    public void resume() {
        if (mNativeTask == 0) return;
        ActorTaskJni.get().resume(mNativeTask);
    }

    /** Similar to pause, explicitly taking control. */
    public void takeOverTask() {
        pause();
    }

    /** Get set of tabs it is acting on. */
    public Set<Integer> getTabs() {
        if (mNativeTask == 0) return new HashSet<>();
        int[] tabIds = ActorTaskJni.get().getTabs(mNativeTask);
        Set<Integer> tabs = new HashSet<>();
        if (tabIds != null) {
            for (int id : tabIds) {
                tabs.add(id);
            }
        }
        return tabs;
    }

    /** Get set of tabs it acted on by the last call to Act. */
    public Set<Integer> getLastActedTabs() {
        if (mNativeTask == 0) return new HashSet<>();
        int[] tabIds = ActorTaskJni.get().getLastActedTabs(mNativeTask);
        Set<Integer> tabs = new HashSet<>();
        if (tabIds != null) {
            for (int id : tabIds) {
                tabs.add(id);
            }
        }
        return tabs;
    }

    /**
     * @return The ID of the tab most recently added or actuated on, or Tab.INVALID_TAB_ID if none.
     *     Unlike {@link #getTabs()} and {@link #getLastActedTabs()}, this ID is preserved after
     *     task completion as long as the underlying tab is still open.
     */
    public int getLastActuatedTabId() {
        if (mNativeTask == 0) return Tab.INVALID_TAB_ID;
        return ActorTaskJni.get().getLastActuatedTabId(mNativeTask);
    }

    /**
     * Returns the target tab ID for bringing to the front, preferring the most recently actuated
     * tab, or falling back to any tab associated with the task. Returns {@link Tab#INVALID_TAB_ID}
     * if none exists.
     */
    public @TabId int getTargetTabId() {
        @TabId int lastActuatedTabId = getLastActuatedTabId();
        if (lastActuatedTabId != Tab.INVALID_TAB_ID) {
            return lastActuatedTabId;
        }
        Set<Integer> tabs = getTabs();
        if (!tabs.isEmpty()) {
            return tabs.iterator().next();
        }
        return Tab.INVALID_TAB_ID;
    }

    /**
     * @param tabId The tab ID to check if the task is acting on.
     * @return true if the task is acting on the given tab, false otherwise.
     */
    public boolean isActingOnTab(int tabId) {
        // TODO(haileywang): This currently loops through all the tabs associated to the task. Look
        // into having native update the latest tabId when the actuated tab changes.
        return isUnderActorControl() && getTabs().contains(tabId);
    }

    /**
     * @return The {@link Profile} associated with this task.
     */
    public @Nullable Profile getProfile() {
        return mProfile.get();
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeTask = 0;
    }

    @NativeMethods
    interface Natives {
        @JniType("std::string")
        String getCurrentActionName(long nativeActorTaskAndroid);

        int getState(long nativeActorTaskAndroid);

        boolean isCompleted(long nativeActorTaskAndroid);

        boolean isUnderActorControl(long nativeActorTaskAndroid);

        void pause(long nativeActorTaskAndroid);

        void resume(long nativeActorTaskAndroid);

        @JniType("std::vector<int32_t>")
        int[] getTabs(long nativeActorTaskAndroid);

        @JniType("std::vector<int32_t>")
        int[] getLastActedTabs(long nativeActorTaskAndroid);

        int getLastActuatedTabId(long nativeActorTaskAndroid);
    }
}
