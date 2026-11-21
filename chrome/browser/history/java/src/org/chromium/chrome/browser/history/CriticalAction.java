// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.critical_actions.ActionType;

import java.util.Objects;

/**
 * A critical action (e.g. a form fill or a password use) performed by the agent during a history
 * visit. Mirrors the user-facing fields of the native {@code
 * critical_actions::CriticalActionEntry}.
 */
@NullMarked
public final class CriticalAction {
    private final String mId;
    private final @ActionType int mActionType;
    private final String mLabel;
    private final String mTooltip;

    /**
     * @param id Unique ID of the critical action.
     * @param actionType Value of the native {@code critical_actions::ActionType} enum.
     * @param label Localized, user-facing label of the action.
     * @param tooltip Localized, user-facing description of the action.
     */
    public CriticalAction(String id, @ActionType int actionType, String label, String tooltip) {
        mId = id;
        mActionType = actionType;
        mLabel = label;
        mTooltip = tooltip;
    }

    /** Returns the unique ID of the critical action. */
    public String getId() {
        return mId;
    }

    /** Returns the value of the native {@code critical_actions::ActionType} enum. */
    public @ActionType int getActionType() {
        return mActionType;
    }

    /** Returns the localized, user-facing label of the action. */
    public String getLabel() {
        return mLabel;
    }

    /** Returns the localized, user-facing description of the action. */
    public String getTooltip() {
        return mTooltip;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof CriticalAction that)) return false;
        return mActionType == that.mActionType
                && Objects.equals(mId, that.mId)
                && Objects.equals(mLabel, that.mLabel)
                && Objects.equals(mTooltip, that.mTooltip);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mId, mActionType, mLabel, mTooltip);
    }
}
