// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/**
 * A handler to trigger seamless navigation / predictive back animation when a back gesture is
 * performed on a navigable tab page.
 */
@JNINamespace("gesturenav")
public class TabOnBackGestureHandler implements UserData {
    private static final Class<TabOnBackGestureHandler> USER_DATA_KEY =
            TabOnBackGestureHandler.class;

    /**
     * @param tab The tab in which page is displayed and back gesture is performed.
     * @return A {@link TabOnBackGestureHandler} to trigger animation on the given tab.
     */
    public static TabOnBackGestureHandler from(Tab tab) {
        var tabOnBackGestureHandler = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (tabOnBackGestureHandler != null) return tabOnBackGestureHandler;
        return tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabOnBackGestureHandler(tab));
    }

    private final long mNativePtr;

    private TabOnBackGestureHandler(Tab tab) {
        mNativePtr = TabOnBackGestureHandlerJni.get().init(tab);
    }

    public void onBackStarted(
            float progress, @BackGestureEventSwipeEdge int edge, boolean forward) {
        TabOnBackGestureHandlerJni.get().onBackStarted(mNativePtr, progress, edge, forward);
    }

    public void onBackProgressed(float progress, @BackGestureEventSwipeEdge int edge) {
        TabOnBackGestureHandlerJni.get().onBackProgressed(mNativePtr, progress, edge);
    }

    public void onBackCancelled() {
        TabOnBackGestureHandlerJni.get().onBackCancelled(mNativePtr);
    }

    public void onBackInvoked() {
        TabOnBackGestureHandlerJni.get().onBackInvoked(mNativePtr);
    }

    public static boolean shouldAnimateNavigationTransition(
            boolean forward, @BackGestureEventSwipeEdge int edge) {
        return TabOnBackGestureHandlerJni.get().shouldAnimateNavigationTransition(forward, edge);
    }

    @Override
    public void destroy() {
        TabOnBackGestureHandlerJni.get().destroy(mNativePtr);
    }

    @NativeMethods
    public interface Natives {
        long init(Tab tab);

        void onBackStarted(
                long nativeTabOnBackGestureHandler, float progress, int edge, boolean forward);

        void onBackProgressed(long nativeTabOnBackGestureHandler, float progress, int edge);

        void onBackCancelled(long nativeTabOnBackGestureHandler);

        void onBackInvoked(long nativeTabOnBackGestureHandler);

        boolean shouldAnimateNavigationTransition(boolean forward, int edge);

        void destroy(long nativeTabOnBackGestureHandler);
    }
}
