// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

/** Interface to listen to the hasContent events from FeedSurfaceMediator. */
public interface HasContentListener {
    /** Called when the has content state changes. */
    void hasContentChanged(@StreamKind int kind, boolean hasContent);
}
