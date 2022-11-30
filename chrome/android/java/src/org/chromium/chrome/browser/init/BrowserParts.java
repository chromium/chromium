// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import androidx.annotation.Nullable;

/**
 * Interface that any {@link AsyncInitializationActivity} can use to interact with this delegate
 * during start up. Functions called by
 * {@link ChromeBrowserInitializer#handlePreNativeStartupAndLoadLibraries(BrowserParts)} are called
 * in the order they are listed.
 */
public interface BrowserParts {
    /**
     * Called during {@link
     * ChromeBrowserInitializer#handlePreNativeStartupAndLoadLibraries(BrowserParts)}. This should
     * consist of java only calls that will not take too much time.
     */
    void preInflationStartup();

    /**
     * Called during {@link
     * ChromeBrowserInitializer#handlePreNativeStartupAndLoadLibraries(BrowserParts)}. It should
     * start layout inflation and also should start loading libraries using {@link
     * NativeInitializationController#startBackgroundTasks}. The {@param
     * onInflationCompleteCallback} should be called once inflation is complete and the content view
     * has been set.
     */
    void setContentViewAndLoadLibrary(Runnable onInflationCompleteCallback);

    /**
     * Called during {@link
     * ChromeBrowserInitializer#handlePreNativeStartupAndLoadLibraries(BrowserParts)}. Early setup
     * after the view hierarchy has been inflated and the background tasks has been initialized. No
     * native calls.
     */
    void postInflationStartup();

    /**
     * Called during {@link ChromeBrowserInitializer#handlePostNativeStartup(BrowserParts)}.
     * Optionaly preconnect to the URL specified in the launch intent, if any. The
     * preconnection is done asynchronously in the native library.
     */
    void maybePreconnect();

    /**
     * Called during {@link ChromeBrowserInitializer#handlePostNativeStartup(BrowserParts)}.
     * Initialize the compositor related classes.
     */
    void initializeCompositor();

    /**
     * Called during {@link ChromeBrowserInitializer#handlePostNativeStartup(BrowserParts)}.
     * Initialize the tab state restoring tabs or creating new tabs.
     */
    void initializeState();

    /**
     * Called during {@link ChromeBrowserInitializer#handlePostNativeStartup(BrowserParts)}.
     * Carry out remaining activity specific tasks for initialization, sub-classes may call
     * finishNativeInitialization asynchronously.
     */
    default void startNativeInitialization() {
        finishNativeInitialization();
    }

    /**
     * Called during {@link ChromeBrowserInitializer#handlePostNativeStartup(BrowserParts)}.
     * Carry out remaining activity specific tasks for initialization
     */
    void finishNativeInitialization();

    /**
     * Called during {@link ChromeBrowserInitializer#handlePostNativeStartup(BrowserParts)} if
     * there was an error during startup.
     * @param failureCause The Exception from the original failure.
     */
    void onStartupFailure(@Nullable Exception failureCause);

    /**
     * @return Whether the activity this delegate represents has been destroyed or is in the
     *         process of finishing.
     */
    boolean isActivityFinishingOrDestroyed();

    /**
     * @return Whether GPU process needs to be started during the startup.
     */
    boolean shouldStartGpuProcess();

    /**
     * @return Whether a minimal browser should be launched during the startup, without running
     *         remaining parts of the Chrome.
     */
    default boolean startMinimalBrowser() {
        return false;
    }
}
