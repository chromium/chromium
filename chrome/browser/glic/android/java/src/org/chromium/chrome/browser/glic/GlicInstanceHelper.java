// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;

/** Helper class for accessing GlicInstanceHelper from Java. */
@NullMarked
@JNINamespace("glic")
public class GlicInstanceHelper {
    /**
     * Interface for observing changes to the GlicInstanceHelper.
     *
     * <p>Observers are notified when the conversation ID or title changes, or when the instance is
     * bound or unbound from the tab.
     */
    @FunctionalInterface
    public interface Observer {
        void onInstanceChanged();
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private String mConversationId = "";
    private String mConversationTitle = "";

    @CalledByNative
    private GlicInstanceHelper() {}

    @CalledByNative
    private void onInstanceChanged(String conversationId, String conversationTitle) {
        mConversationId = conversationId;
        mConversationTitle = conversationTitle;
        for (Observer observer : mObservers) {
            observer.onInstanceChanged();
        }
    }

    /**
     * Returns the {@link GlicInstanceHelper} for the given {@link Tab}.
     *
     * @param tab The {@link Tab} to get the {@link GlicInstanceHelper} for.
     * @return The {@link GlicInstanceHelper} for the given {@link Tab}.
     */
    public static GlicInstanceHelper from(Tab tab) {
        return GlicInstanceHelperJni.get().getForTab(tab);
    }

    /**
     * Sets the {@link Natives} for testing.
     *
     * @param natives The {@link Natives} to set for testing. This is used to mock the native
     *     methods of the {@link GlicInstanceHelper} in unit tests.
     */
    public static void setNativesForTesting(Natives natives) {
        GlicInstanceHelperJni.setInstanceForTesting(natives);
    }

    /**
     * Returns the conversation ID for the current instance.
     *
     * @return The conversation ID for the current instance.
     */
    public String getConversationId() {
        return mConversationId;
    }

    /**
     * Returns the conversation title for the current instance.
     *
     * @return The conversation title for the current instance.
     */
    public String getConversationTitle() {
        return mConversationTitle;
    }

    /**
     * Adds an observer to the list of observers.
     *
     * @param observer The observer to add.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer from the list of observers.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Interface containing the native methods for {@link GlicInstanceHelper} that are implemented
     * in C++.
     *
     * <p>This also allows mocking these methods in unit tests.
     */
    @NativeMethods
    public interface Natives {
        GlicInstanceHelper getForTab(Tab tab);
    }
}
