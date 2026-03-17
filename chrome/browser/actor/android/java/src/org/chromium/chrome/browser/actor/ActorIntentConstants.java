// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;

/** Constants used for Intents and Broadcasts related to Actor tasks. */
@NullMarked
public final class ActorIntentConstants {
    /** Extra for the unique task ID associated with the intent. */
    public static final String EXTRA_TASK_ID = "org.chromium.chrome.browser.actor.EXTRA_TASK_ID";

    /** Extra to pause task. */
    public static final String ACTION_PAUSE = "org.chromium.chrome.browser.actor.ACTION_PAUSE";

    /** Extra to resume task. */
    public static final String ACTION_RESUME = "org.chromium.chrome.browser.actor.ACTION_RESUME";

    /** Request Code to receive broadcast. */
    public static final int REQUEST_CODE_PAUSE_RESUME = 101;

    // Prevent instantiation
    private ActorIntentConstants() {}
}
