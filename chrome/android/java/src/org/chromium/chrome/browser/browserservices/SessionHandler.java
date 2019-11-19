// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.widget.RemoteViews;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSessionToken;

/**
 * Interface to handle browser services calls whenever the session id matched.
 * TODO(yusufo): Add a way to handle mayLaunchUrl as well.
 */
public interface SessionHandler {

    /**
     * @return The session this {@link SessionHandler} is associated with.
     */
    CustomTabsSessionToken getSession();

    /**
     * Finds the action button with the given id, and updates it with the new content.
     * @return Whether the action button has been updated.
     */
    boolean updateCustomButton(int id, Bitmap bitmap, String description);

    /**
     * Updates the {@link RemoteViews} shown on the secondary toolbar.
     * @return Whether this update is successful.
     */
    boolean updateRemoteViews(
            RemoteViews remoteViews, int[] clickableIDs, PendingIntent pendingIntent);

    /**
     * @return The current url being displayed to the user.
     */
    @Nullable String getCurrentUrl();

    /**
     * @return The url of a pending navigation, if any.
     */
    @Nullable String getPendingUrl();

    /**
     * @return the task id the content handler is running in.
     */
    int getTaskId();

    /**
     * @return the class of the Activity the content handler is running in.
     */
    Class<? extends Activity> getActivityClass();

    /**
     * Attempts to handles a new intent (without starting a new activity).
     * Returns whether has handled.
     */
    boolean handleIntent(Intent intent);

    /**
     * Checks whether the given referrer can be used as valid within the Activity hosting this
     * handler.
     */
    boolean canUseReferrer(Uri referrer);
}
