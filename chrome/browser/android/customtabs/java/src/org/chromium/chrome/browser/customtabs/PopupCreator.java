// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.WebContents;

/** Interface for creating and managing popups. */
@NullMarked
public interface PopupCreator {
    /**
     * Creates a new popup window and starts the activity. Note: Trusted intent extras are
     * automatically added to the intent within this method.
     *
     * @param context The context used to start the activity.
     * @param isIncognito Whether the popup is for an incognito tab.
     * @param windowFeatures The requested window features.
     * @param additionalIntentExtras Additional extras to add to the intent.
     * @param startActivityOptions Options to pass to startActivity.
     */
    void createPopupWindow(
            Context context,
            boolean isIncognito,
            @Nullable WindowFeatures windowFeatures,
            @Nullable Bundle additionalIntentExtras,
            @Nullable Bundle startActivityOptions);

    /**
     * Moves the given {@link Tab} to a new Custom Tab popup window.
     *
     * @param tab The {@link Tab} to move.
     * @param windowFeatures The {@link WindowFeatures} to use for the new Custom Tab popup window.
     * @return {@code true} if the tab was successfully reparented to a new movable Task, {@code
     *     false} otherwise
     */
    boolean moveTabToNewPopup(Tab tab, WindowFeatures windowFeatures);

    /**
     * Moves the given {@link WebContents} to a new Document Picture-in-Picture window.
     *
     * <p>Note: The {@code windowBounds} specified in {@code windowOptions} may be overridden by
     * cached user-resized bounds from a previous PiP window of the same origin if available, to
     * provide size persistence.
     *
     * @param srcActivity The {@link Activity} that initiated the Document Picture-in-Picture
     *     request.
     * @param webContents The {@link WebContents} to move.
     * @param windowOptions The {@link PictureInPictureWindowOptions} to use for the new Document
     *     Picture-in-Picture window.
     */
    boolean moveWebContentsToNewDocumentPictureInPictureWindow(
            @Nullable Activity srcActivity,
            WebContents webContents,
            PictureInPictureWindowOptions windowOptions);

    /**
     * Starts an activity using given {@link android.content.Context}, {@link
     * android.content.Intent}, and {@link android.os.Bundle} of ActivityOptions. Catches exceptions
     * likely to be thrown when {@link android.app.ActivityOptions#setMovableTaskRequired(boolean)}
     * is set to {@code true} in the {@link android.app.ActivityOptions} object represented by the
     * {@link android.os.Bundle} provided.
     *
     * @param context The Context on which the {@link
     *     android.content.Context#startActivity(android.content.Intent, android.os.Bundle)} method
     *     will be executed.
     * @param intent The Intent passed to the {@code startActivity} call.
     * @param activityOptions The Bundle passed to the {@code startActivity} call.
     * @return {@code true} if succeeded, {@code false} otherwise.
     * @see android.app.ActivityOptions#toBundle()
     * @see android.app.ActivityOptions#setMovableTaskRequired(boolean)
     */
    boolean tryStartActivity(Context context, Intent intent, @Nullable Bundle activityOptions);
}
