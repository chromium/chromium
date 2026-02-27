// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabwindow;

import org.jni_zero.CalledByNative;

import org.chromium.base.JniOnceRunnable;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;

/** Support class for accessing TabWindowManager in tests from native code. */
public class TabWindowManagerNativeTestSupport {
    private static final String TAG = "TWMNTS";

    /** Returns the window ID associated with the given TabModel. */
    @CalledByNative
    public static @WindowId int getWindowIdForModel(TabModel targetModel) {
        TabWindowManager manager = TabWindowManagerSingleton.getInstance();
        assert manager != null;

        for (TabModelSelector selector : manager.getAllTabModelSelectors()) {
            for (TabModel model : selector.getModels()) {
                if (model == targetModel) {
                    int id = manager.getWindowIdForSelector(selector);
                    Log.i(TAG, "Window id found for model: " + id);
                    return id;
                }
            }
        }
        Log.e(TAG, "Failed to find window ID for model");
        return TabWindowManager.INVALID_WINDOW_ID;
    }

    /** Waits for the TabModelSelector with the given window ID to be created and initialized. */
    @CalledByNative
    public static void waitForTabModelSelectorWithId(int windowId, JniOnceRunnable quitRunnable) {
        TabWindowManager manager = TabWindowManagerSingleton.getInstance();
        assert manager != null;

        Log.i(TAG, "Waiting for TabModelSelector with id " + windowId);

        TabModelSelector existingSelector = manager.getTabModelSelectorById(windowId);
        if (existingSelector != null) {
            Log.i(TAG, "TabModelSelector already exists for id " + windowId);
            runOnTabStateInitialized(quitRunnable, existingSelector);
            return;
        }
        manager.addObserver(
                new TabWindowManager.Observer() {
                    @Override
                    public void onTabModelSelectorAdded(TabModelSelector selector) {
                        @WindowId int id = manager.getWindowIdForSelector(selector);
                        Log.i(TAG, "TabModelSelector added " + id);

                        if (id == windowId) {
                            manager.removeObserver(this);
                            runOnTabStateInitialized(quitRunnable, selector);
                        }
                    }
                });
    }

    private static void runOnTabStateInitialized(
            JniOnceRunnable runnable, TabModelSelector selector) {
        Runnable innerRunnable =
                () -> {
                    Log.i(TAG, "Tab state initialized for selector. Running runnable.");
                    // Runnable's native side is implicitly deleted by this call.
                    runnable.run();
                };
        // Post the runnable to the UI thread so it is always executed async.
        TabModelUtils.runOnTabStateInitialized(
                () -> ThreadUtils.runOnUiThread(innerRunnable), selector);
    }
}
