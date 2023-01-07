// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.scene;

/**
 * An interface that surfaces functions for changing the lightweight reactions scene.
 */
interface SceneEditorDelegate {
    /**
     * Returns whether the user should be able to add a reaction to the scene.
     */
    boolean canAddReaction();

    /**
     * Duplicates the given {@link ReactionLayout} to the scene (offsetting if necessary).
     */
    void duplicateReaction(ReactionLayout reactionLayout);

    /**
     * Shows the {@link org.chromium.ui.widget.Toast} that indicates the max number of reactions on
     * the scene has been reached.
     */
    void showMaxReactionsReachedToast();

    /**
     * Removes the given {@link ReactionLayout} from the scene.
     */
    void removeReaction(ReactionLayout reactionLayout);

    /**
     * Marks the given {@link ReactionLayout}'s active status as {@code isActive}.
     */
    void markActiveStatus(ReactionLayout reactionLayout, boolean isActive);

    /**
     * Invoked when a reaction is dragged across the editing surface.
     */
    void reactionWasMoved(ReactionLayout reactionLayout);

    /**
     * Invoked when the scale / rotate editing control of a reaction is interacted with.
     */
    void reactionWasAdjusted();
}
