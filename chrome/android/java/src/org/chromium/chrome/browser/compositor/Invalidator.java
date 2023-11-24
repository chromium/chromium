// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

/** The {@link Invalidator} invalidates a client when it is the right time. */
public class Invalidator {
    /** Interface for the host that drives the invalidations. */
    public interface Host {
        /**
         * Requests an invalidation of the view.
         *
         * @param invalidator {@link Runnable} that invalidates the view.
         */
        void deferInvalidate(Runnable invalidator);
    }

    private Host mHost;

    /**
     * @param host The invalidator host, responsible for invalidating views.
     */
    public void set(Host host) {
        mHost = host;
    }

    /**
     * Invalidates either immediately (if no host is specified) or at time
     * triggered by the host.
     *
     * @param invalidator The {@link Runnable} performing invalidation.
     */
    public void invalidate(Runnable invalidator) {
        if (mHost != null) {
            mHost.deferInvalidate(invalidator);
        } else {
            invalidator.run();
        }
    }
}
