// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.graphics.drawable.Drawable;
import android.view.Surface;
import android.view.View;

/**
 * Manages Surface(s), and SurfaceView(s) when necessary, for the compositor.
 *
 * See CompositorSurfaceManagerImpl for the standard implementation of this class.
 * While in VR, the VRCompositorSurfaceManager will be used, which only partially implements the
 * contract implied by this interface. See the comments there for details.
 */
public interface CompositorSurfaceManager {
    /**
     * Delivers Surface lifecycle events to the target of this CompositorSurfaceManager.
     */
    public interface SurfaceManagerCallbackTarget {
        public void surfaceRedrawNeededAsync(Runnable drawingFinished);
        public void surfaceChanged(Surface surface, int format, int width, int height);
        public void surfaceCreated(Surface surface);
        public void surfaceDestroyed(Surface surface);
        public void unownedSurfaceDestroyed();
    }

    /**
     * Turn off everything.
     */
    void shutDown();

    /**
     * Called by the client to request a surface. Once called, we guarantee that the next call to
     * surfaceCreated will match the most recent value of |format|. If the surface is already
     * available for use, then we'll send synthetic callbacks as though it were destroyed and
     * recreated. Note that |format| must be either OPAQUE or TRANSLUCENT.
     */
    void requestSurface(int format);

    /**
     * Called to notify us that the client no longer needs the surface that it doesn't own. This
     * tells us that we may destroy it. Note that it's okay if it never had an unowned surface.
     */
    void doneWithUnownedSurface();

    /**
     * Destroy and re-create the surface.
     */
    void recreateSurface();

    /**
     * Update the background drawable on all surfaces.
     */
    void setBackgroundDrawable(Drawable background);

    /**
     * Set |willNotDraw| on all surfaces.
     */
    void setWillNotDraw(boolean willNotDraw);

    /**
     * Set the visibility of the Managed SurfaceViews.
     */
    void setVisibility(int visibility);

    /**
     * Gets the active {@link SurfaceView}.
     */
    View getActiveSurfaceView();
}
