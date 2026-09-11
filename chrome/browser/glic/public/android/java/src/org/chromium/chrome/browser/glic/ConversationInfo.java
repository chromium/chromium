// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;

/** Lightweight description of a recent Glic conversation, populated from native. */
@NullMarked
public class ConversationInfo {
    /** Opaque native instance id used to target the conversation. */
    public final String instanceId;

    /** Human-readable conversation title shown in the menu. */
    public final String title;

    /**
     * @param instanceId Opaque native instance id used to target the conversation.
     * @param title Human-readable conversation title shown in the menu.
     */
    public ConversationInfo(String instanceId, String title) {
        this.instanceId = instanceId;
        this.title = title;
    }
}
