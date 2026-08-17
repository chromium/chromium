// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/**
 * A handler to trigger seamless navigation / predictive back animation when a back gesture is
 * performed on a navigable tab page.
 */
@JNINamespace("gesturenav")
@NullMarked
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

    private long mNativePtr;

    private TabOnBackGestureHandler(Tab tab) {
        mNativePtr = TabOnBackGestureHandlerJni.get().init(tab);
    }

    public void onBackStarted(
            float progress,
            @BackGestureEventSwipeEdge int edge,
            boolean forward,
            boolean isGestureMode) {
        if (mNativePtr == 0) return;
        TabOnBackGestureHandlerJni.get()
                .onBackStarted(mNativePtr, progress, edge, forward, isGestureMode);
    }

    /**
     * Returns whether this handler is still driving the caller's gesture. When it returns false the
     * caller owns the gesture again: it must drop its reference to this handler and do its own back
     * handling, otherwise the navigation is lost. The handler is not necessarily idle then: on an
     * edge mismatch it may still be driving a newer gesture for another owner.
     */
    public boolean onBackProgressed(
            float progress,
            @BackGestureEventSwipeEdge int edge,
            boolean forward,
            boolean isGestureMode) {
        if (mNativePtr == 0) return false;
        return TabOnBackGestureHandlerJni.get()
                .onBackProgressed(mNativePtr, progress, edge, forward, isGestureMode);
    }

    public void onBackCancelled(boolean isGestureMode) {
        if (mNativePtr == 0) return;
        TabOnBackGestureHandlerJni.get().onBackCancelled(mNativePtr, isGestureMode);
    }

    public void onBackInvoked(boolean isGestureMode) {
        if (mNativePtr == 0) return;
        TabOnBackGestureHandlerJni.get().onBackInvoked(mNativePtr, isGestureMode);
    }

    public static boolean shouldAnimateNavigationTransition(
            boolean forward, @BackGestureEventSwipeEdge int edge) {
        return TabOnBackGestureHandlerJni.get().shouldAnimateNavigationTransition(forward, edge);
    }

    @Override
    public void destroy() {
        if (mNativePtr != 0) {
            TabOnBackGestureHandlerJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
    }

    @NativeMethods
    public interface Natives {
        long init(Tab tab);

        void onBackStarted(
                long nativeTabOnBackGestureHandler,
                float progress,
                int edge,
                boolean forward,
                boolean isGestureMode);

        boolean onBackProgressed(
                long nativeTabOnBackGestureHandler,
                float progress,
                int edge,
                boolean forward,
                boolean isGestureMode);

        void onBackCancelled(long nativeTabOnBackGestureHandler, boolean isGestureMode);

        void onBackInvoked(long nativeTabOnBackGestureHandler, boolean isGestureMode);

        boolean shouldAnimateNavigationTransition(boolean forward, int edge);

        void destroy(long nativeTabOnBackGestureHandler);
    }
}
