// Copyright 2021 The Chromium Authors
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

    void onActivityPaused();

    void onActivityResumed();

    /** Enumeration of the possible selection options of feed tabs. */
    @IntDef({StreamTabId.DEFAULT, StreamTabId.FOR_YOU, StreamTabId.FOLLOWING})
    public @interface StreamTabId {
        /**
         * Used for NTP restore operations, when it may be desirable to recover the previous tab
         * selection.
         */
        int DEFAULT = -1;

        /** Selects the For you feed tab. */
        int FOR_YOU = 0;

        /** Selects the Following feed tab. */
        int FOLLOWING = 1;
    };

    void restoreInstanceState(String state);

    String getSavedInstanceStateString();
}
