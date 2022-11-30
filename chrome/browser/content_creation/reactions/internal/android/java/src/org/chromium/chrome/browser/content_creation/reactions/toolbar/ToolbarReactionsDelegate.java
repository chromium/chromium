// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.toolbar;

import org.chromium.components.content_creation.reactions.ReactionMetadata;

/**
 * Interface used by the toolbar to delegate action handlers for tapping reactions.
 */
public interface ToolbarReactionsDelegate {
    /**
     * Invoked when the user taps a reaction.
     */
    public void onToolbarReactionTapped(ReactionMetadata reactionData);
}