// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

/** Interface for Setup List modules to report their completion status. */
@NullMarked
public interface SetupListCompletable {
    /**
     * Checks if the setup task associated with this module is complete.
     *
     * @return True if the task is complete, false otherwise.
     */
    boolean isComplete();

    /**
     * Returns the icon to be displayed when the setup task is complete.
     *
     * @return The drawable resource ID for the completed state icon.
     */
    @DrawableRes
    int getCardImageCompletedResId();

    /** Helper class to hold UI information related to completion state. */
    class CompletionState {
        public final @DrawableRes int iconRes;
        public final boolean isCompleted;

        public CompletionState(@DrawableRes int iconRes, boolean isCompleted) {
            this.iconRes = iconRes;
            this.isCompleted = isCompleted;
        }
    }

    /**
     * Gets the appropriate UI information for a given card provider, considering completion state.
     *
     * @param provider The EducationalTipCardProvider instance.
     * @return CompletionState containing the drawable resource and completion status.
     */
    static @Nullable CompletionState getCompletionState(
            EducationalTipCardProvider provider, @ModuleType int moduleType) {
        if (!SetupListModuleUtils.isSetupListModule(moduleType)) {
            return null;
        }
        if (provider instanceof SetupListCompletable completable && completable.isComplete()) {
            // Setup list module completed by the user.
            // If the module is awaiting its animation, we should still show it as NOT completed
            // initially so the user can see the transition.
            if (SetupListModuleUtils.isModuleAwaitingCompletionAnimation(moduleType)) {
                return new CompletionState(provider.getCardImage(), /* isCompleted= */ false);
            }

            return new CompletionState(
                    completable.getCardImageCompletedResId(), /* isCompleted= */ true);
        }
        // Setup list module that is yet to be completed by the user
        return new CompletionState(provider.getCardImage(), /* isCompleted= */ false);
    }
}
