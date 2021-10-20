// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.componentinterfaces;

import androidx.annotation.IntDef;

/** Interface for referencing FeedSurfaceCoordinator in this library. */
public interface SurfaceCoordinator {
    /** Observes the SurfaceCoordinator. */
    interface Observer {
        default void surfaceOpened() {}
    }
    void addObserver(Observer observer);
    void removeObserver(Observer observer);

    void onSurfaceClosed();
    void onSurfaceOpened();
    boolean isActive();

    @IntDef({StreamTabId.DEFAULT, StreamTabId.FOR_YOU, StreamTabId.FOLLOWING})
    public @interface StreamTabId {
        int DEFAULT = -1;
        int FOR_YOU = 0;
        int FOLLOWING = 1;
    };
}
