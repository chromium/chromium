// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import android.graphics.Rect;

import org.chromium.build.annotations.NullMarked;

/** An interface for notifications about the state of the message container */
@NullMarked
public interface MessageContainerObserver {
    /**
     * A notification that the message container has been shown.
     *
     * @param bounds The message container view rect.
     */
    void onShowMessageContainer(int viewId, Rect rect);

    /**
     * A notification that the message container has been hidden.
     *
     * @param viewId The ID of the message container view.
     */
    void onHideMessageContainer(int viewId);
}
