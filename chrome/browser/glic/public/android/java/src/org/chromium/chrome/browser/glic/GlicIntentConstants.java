// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;

/** Public constants for external intents interacting with Glic (e.g. from the Gemini app). */
@NullMarked
public final class GlicIntentConstants {
    /** Action for external intents triggering Glic background execution or task interrupts. */
    public static final String ACTION_EXTERNAL_TRIGGERING =
            "org.chromium.chrome.browser.glic.EXTERNAL_TRIGGERING";

    /** Extra containing the Glic conversation ID string for task interrupt / routing. */
    public static final String EXTRA_CONVERSATION_ID =
            "org.chromium.chrome.browser.glic.CONVERSATION_ID";

    /**
     * Boolean extra indicating that a Glic external trigger fell back to foregrounding Chrome and a
     * new actor task is pending creation.
     */
    public static final String EXTRA_GLIC_PENDING_ACTOR_TASK =
            "org.chromium.chrome.browser.glic.PENDING_ACTOR_TASK";

    private GlicIntentConstants() {}
}
