// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

import java.util.HashSet;
import java.util.Set;

/** Represents an ongoing actor interaction. This class is a wrapper around the native ActorTask. */
@JNINamespace("actor")
@NullMarked
public class ActorTask {
    private long mNativeTask;
    private final int mId;
    private final String mTitle;

    @CalledByNative
    private ActorTask(long nativeTask, int id, String title) {
        mNativeTask = nativeTask;
        mId = id;
        mTitle = title;
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

    /**
     * @param tabId The tab ID to check if the task is acting on.
     * @return true if the task is acting on the given tab, false otherwise.
     */
    public boolean isActingOnTab(int tabId) {
        // TODO(haileywang): This currently loops through all the tabs associated to the task. Look
        // into having native update the latest tabId when the actuated tab changes.
        return isUnderActorControl() && getTabs().contains(tabId);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeTask = 0;
    }

    @NativeMethods
    interface Natives {
        String getCurrentActionName(long nativeActorTaskAndroid);

        int getState(long nativeActorTaskAndroid);

        boolean isCompleted(long nativeActorTaskAndroid);

        boolean isUnderActorControl(long nativeActorTaskAndroid);

        void pause(long nativeActorTaskAndroid);

        void resume(long nativeActorTaskAndroid);

        int[] getTabs(long nativeActorTaskAndroid);
    }
}
