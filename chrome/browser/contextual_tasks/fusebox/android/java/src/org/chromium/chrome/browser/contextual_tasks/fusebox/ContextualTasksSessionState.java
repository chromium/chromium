// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.fusebox;

import android.content.Context;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/**
 * Fusebox session state for Contextual Tasks.
 *
 * <p>Unlike the default FuseboxSessionState, this subclass prevents automatic deactivation when
 * focus is lost, ensuring that the AI conversation and attachments persist as long as the task is
 * active.
 */
@NullMarked
public class ContextualTasksSessionState extends FuseboxSessionState {
    @Override
    public boolean isTaskScoped() {
        return true;
    }

    @Override
    public void activate(
            Context context,
            @Nullable WebContents webContents,
            MonotonicObservableSupplier<Profile> profileSupplier,
            @Nullable Runnable onFullyActivated) {
        assert webContents != null
                : "WebContents must not be null for Contextual Tasks activation.";
        @Nullable WebContents current = getContextualTasksWebContents();
        if (current != null) {
            assert current == webContents : "ContextualTasks WebContents should never change.";
        }
        super.activate(context, webContents, profileSupplier, onFullyActivated);
    }

    @Override
    public void deactivate() {
        // No-op: Prevent LocationBarMediator from clearing the session when focus is lost.
        // Contextual Tasks manages its own session lifecycle in ContextualTasksFuseboxManager.
    }

    @Override
    protected void createAutoComplete(Profile profile) {
        mAutocomplete = null;
    }

    @Override
    public void destroy() {
        // We override deactivate() to be a no-op, so we must explicitly call the base
        // implementation during destruction to ensure cleanup.
        super.deactivate();
        super.destroy();
    }
}
