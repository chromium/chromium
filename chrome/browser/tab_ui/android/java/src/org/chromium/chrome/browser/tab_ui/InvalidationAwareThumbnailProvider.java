// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.graphics.Canvas;

/**
 * A thumbnail provider that is aware of the visual state of the thumbnail and only requests
 * thumbnail capturing if that has changed since the last capture.
 */
public interface InvalidationAwareThumbnailProvider {

    /**
     * @return Whether a thumbnail should be captured for this provider.  Should only happen if
     *         the contents have changed since the last thumbnail capture.
     */
    boolean shouldCaptureThumbnail();

    /**
     * Captures the contents of this provider into the specified output.  At this point, the
     * provider should keep track of all the properties that determine its visual state and use
     * those in calls to {@link #shouldCaptureThumbnail()} to prevent redundant thumbnail captures.
     * @param canvas The output to draw the thumbnail to.
     */
    void captureThumbnail(Canvas canvas);
}
