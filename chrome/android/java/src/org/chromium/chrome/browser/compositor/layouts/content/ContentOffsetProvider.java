// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

/**
 * An interface representing the offset of the content.
 * TODO: This should be refactored into the FullScreenManager.
 */
public interface ContentOffsetProvider {
    /**
     * @return How far to translate any Android overlay views to recreate the correct content data
     *         in px.
     */
    public float getOverlayTranslateY();
}
