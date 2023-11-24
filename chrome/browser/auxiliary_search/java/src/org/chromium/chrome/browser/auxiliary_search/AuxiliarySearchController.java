// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;

/** This Controller for the auxiliary search. */
public interface AuxiliarySearchController extends PauseResumeWithNativeObserver {
    /**
     * Registers to the given lifecycle dispatcher.
     * @param lifecycleDispatcher tracks the lifecycle of the Activity of pause and resume.
     */
    default void register(ActivityLifecycleDispatcher lifecycleDispatcher) {}

    @Override
    default void onResumeWithNative() {}

    @Override
    default void onPauseWithNative() {}

    /** Destroy and unhook objects at destruction. */
    default void destroy() {}
}
