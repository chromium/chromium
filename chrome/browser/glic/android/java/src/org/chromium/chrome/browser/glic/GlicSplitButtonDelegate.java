// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;

/**
 * Java equivalent of the C++ GlicSplitButtonDelegate for managing Glic split button UI states,
 * nudges, and related entry points.
 */
@NullMarked
public interface GlicSplitButtonDelegate {

    /**
     * Called to trigger/show the Glic nudge UI.
     *
     * @param label The action label. This string appears on the clickable part of the nudge.
     * @param anchoredMessageText The longer description, shown in the anchored message UI.
     * @param promptSuggestion The optional prompt to be filled in to Glic if the nudge is clicked.
     */
    default void onTriggerGlicNudgeUi(
            String label, String anchoredMessageText, String promptSuggestion) {}

    /** Called to hide/dismiss the Glic nudge UI. */
    default void onHideGlicNudgeUi() {}

    /** Returns whether the Glic nudge UI is currently showing. */
    default boolean getIsShowingGlicNudge() {
        return false;
    }

    /**
     * Called when native C++ updates whether the Glic button should be shown.
     *
     * @param show True if the Glic button should be shown in the UI.
     */
    default void setGlicShowState(boolean show) {}

    /**
     * Called when native C++ updates whether the Glic panel UI is open.
     *
     * @param open True if the Glic panel is currently open.
     */
    default void setGlicPanelIsOpen(boolean open) {}

    /** Called when native C++ requests showing the Glic actor task icon. */
    default void showGlicActorTaskIcon() {}

    /** Called when native C++ requests hiding the Glic actor task icon. */
    default void hideGlicActorTaskIcon() {}

    /** Returns whether the Glic actor task icon is showing with nudge text. */
    default boolean getIsShowingGlicActorTaskIconNudge() {
        return false;
    }

    /**
     * Called when native C++ updates the Glic actor nudge label.
     *
     * @param nudgeLabel The text label for the actor nudge.
     */
    default void setGlicActorNudgeLabel(String nudgeLabel) {}

    /**
     * Called when native C++ triggers an actor nudge with text.
     *
     * @param nudgeText The nudge text to show.
     */
    default void triggerGlicActorNudge(String nudgeText) {}

    /**
     * Called when native C++ updates the actor nudge button pressed state.
     *
     * @param pressed True if the actor task list bubble is open / pressed.
     */
    default void setGlicActorNudgePressedState(boolean pressed) {}

    /** Called when native C++ requests showing the actor task list bubble / menu. */
    default void showActorTaskListBubble() {}

    /** Called when native C++ requests closing the actor task list bubble / menu. */
    default void closeActorTaskListBubble() {}

    /** Returns whether the actor task list bubble / menu is currently showing. */
    default boolean isActorTaskListBubbleShowing() {
        return false;
    }
}
