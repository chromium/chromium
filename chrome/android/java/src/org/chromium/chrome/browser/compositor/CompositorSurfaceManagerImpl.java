// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.content.Context;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.Log;

/**
 * Manage multiple SurfaceViews for the compositor, so that transitions between
 * surfaces with and without an alpha channel can be visually smooth.
 *
 * This class allows a client to request a 'translucent' or 'opaque' surface, and we will signal via
 * SurfaceHolder.Callback when it's ready.  We guarantee that the client will receive surfaceCreated
 * / surfaceDestroyed only for a surface that represents the most recently requested PixelFormat.
 *
 * Internally, we maintain two SurfaceViews, since calling setFormat() to change the PixelFormat
 * results in a visual glitch as the surface is torn down.  crbug.com/679902
 *
 * The client has the responsibility to call doneWithUnownedSurface() at some point between when we
 * call back its surfaceCreated, when it is safe for us to hide the SurfaceView with the wrong
 * format.  It is okay if it requests multiple surfaces without calling doneWithUnownedSurface.
 *
 * If the client requests the same format more than once in a row, it will still receive destroyed /
 * created / changed messages for it, even though we won't tear it down.
 *
 * The full design doc is at https://goo.gl/aAmQzR .
 */
class CompositorSurfaceManagerImpl implements SurfaceHolder.Callback2, CompositorSurfaceManager {
    private static class SurfaceState {
        public SurfaceView surfaceView;

        // Do we expect a surfaceCreated?
        public boolean createPending;

        // Have we started destroying |surfaceView|, but haven't received surfaceDestroyed yet?
        public boolean destroyPending;

        // Last PixelFormat that we received, or UNKNOWN if we don't know / don't want to cache it.
        public int format;

        // Last known width, height for thsi surface.
        public int width;
        public int height;

        // Parent ViewGroup, or null.
        private ViewGroup mParent;

        public SurfaceState(Context context, int format, SurfaceHolder.Callback2 callback) {
            surfaceView = new SurfaceView(context);

            // Media overlays require a translucent surface for the compositor which should be
            // placed above them, so we mark it setZOrderMediaOverlay. But its not not needed for
            // the opaque one. In fact setting this for the opaque one causes glitches when
            // transitioning to the opaque SurfaceView. This is because if the opaque SurfaceView is
            // stacked on top of the translucent one, the framework doesn't draw any content
            // underneath it and shows its background instead when it has no content during the
            // transition.
            if (format == PixelFormat.TRANSLUCENT) surfaceView.setZOrderMediaOverlay(true);
            surfaceView.setVisibility(View.INVISIBLE);
            surfaceHolder().setFormat(format);
            surfaceHolder().addCallback(callback);

            // Set this to UNKNOWN until we get a format back.
            this.format = PixelFormat.UNKNOWN;
        }

        public SurfaceHolder surfaceHolder() {
            return surfaceView.getHolder();
        }

        public boolean isValid() {
            return surfaceHolder().getSurface().isValid();
        }

        // Attach to |parent|, such that isAttached() will be correct immediately.  Otherwise,
        // attaching and detaching can cause surfaceCreated / surfaceDestroyed callbacks without
        // View.hasParent being up to date.
        public void attachTo(ViewGroup parent, FrameLayout.LayoutParams lp) {
            mParent = parent;
            mParent.addView(surfaceView, lp);
        }

        public void detachFromParent() {
            Log.e(TAG, "SurfaceState : detach from parent : " + format);
            final ViewGroup parent = mParent;
            // Since removeView can call surfaceDestroyed before returning, be sure that isAttached
            // will return false.
            mParent = null;
            parent.removeView(surfaceView);
        }

        public boolean isAttached() {
            return mParent != null;
        }
    }

    private static final String TAG = "CompositorSurfaceMgr";

    // SurfaceView with a translucent PixelFormat.
    private final SurfaceState mTranslucent;

    // SurfaceView with an opaque PixelFormat.
    private final SurfaceState mOpaque;

    // Surface that we last gave to the client with surfaceCreated.  Cleared when we call
    // surfaceDestroyed on |mClient|.  Note that it's not necessary that Android has notified us
    // the surface has been destroyed; we deliberately keep it around until the client tells us that
    // it's okay to get rid of it.
    private SurfaceState mOwnedByClient;

    // Surface that was most recently requested by the client.
    private SurfaceState mRequestedByClient;

    // Client that we notify about surface change events.
    private SurfaceManagerCallbackTarget mClient;

    // View to which we'll attach the SurfaceView.
    private final ViewGroup mParentView;

    public CompositorSurfaceManagerImpl(ViewGroup parentView, SurfaceManagerCallbackTarget client) {
        mParentView = parentView;
        mClient = client;

        mTranslucent = new SurfaceState(parentView.getContext(), PixelFormat.TRANSLUCENT, this);
        mOpaque = new SurfaceState(mParentView.getContext(), PixelFormat.OPAQUE, this);
    }

    /**
     * Turn off everything.
     */
    @Override
    public void shutDown() {
        mRequestedByClient = null;
        detachSurfaceNow(mOpaque);
        detachSurfaceNow(mTranslucent);

        mTranslucent.surfaceHolder().removeCallback(this);
        mOpaque.surfaceHolder().removeCallback(this);
    }

    @Override
    public void requestSurface(int format) {
        Log.e(TAG, "Transitioning to surface with format : " + format);
        mRequestedByClient = (format == PixelFormat.TRANSLUCENT) ? mTranslucent : mOpaque;

        // If destruction is pending, then we must wait for it to complete.  When we're notified
        // that it is destroyed, we'll re-start construction if the client still wants this surface.
        // Note that we could send a surfaceDestroyed for the owned surface, if there is one, but we
        // defer it until later so that the client can still use it until the new one is ready.
        if (mRequestedByClient.destroyPending) return;

        // The requested surface isn't being torn down.

        // If the surface isn't attached yet, then attach it.  Otherwise, we're still waiting for
        // the surface to be created, or we've already received surfaceCreated for it.
        if (!mRequestedByClient.isAttached()) {
            attachSurfaceNow(mRequestedByClient);
            assert mRequestedByClient.isAttached();
            return;
        }

        // Surface is not pending destroy, and is attached.  See if we need to send any synthetic
        // callbacks to the client.  If we're expecting a callback from Android, then we'll handle
        // it when it arrives instead.
        if (mRequestedByClient.createPending) return;

        // Surface is attached and no create is pending.  Send a synthetic create.  Note that, if
        // Android destroyed the surface itself, then we'd have set |createPending| at that point.
        // We don't check |isValid| here, since, technically, there could be a destroy in flight
        // from Android.  It's okay; we'll just notify the client at that point.  Either way, we
        // must tell the client that it now owns the surface.

        // Send a notification about any owned surface.  Note that this can be |mRequestedByClient|
        // which is fine.  We'll send destroy / create for it.  Also note that we don't actually
        // start tear-down of the owned surface; the client notifies us via doneWithUnownedSurface
        // when it is safe to do that.
        disownClientSurface(mOwnedByClient);

        // The client now owns |mRequestedByClient|.  Notify it that it's ready.
        mOwnedByClient = mRequestedByClient;
        mClient.surfaceCreated(mOwnedByClient.surfaceHolder().getSurface());

        // See if we're expecting a surfaceChanged.  If not, then send a synthetic one.
        if (mOwnedByClient.format != PixelFormat.UNKNOWN) {
            mClient.surfaceChanged(mOwnedByClient.surfaceHolder().getSurface(),
                    mOwnedByClient.format, mOwnedByClient.width, mOwnedByClient.height);
        }
    }

    @Override
    public void doneWithUnownedSurface() {
        if (mOwnedByClient == null) return;

        SurfaceState unowned = (mOwnedByClient == mTranslucent) ? mOpaque : mTranslucent;

        if (mRequestedByClient == unowned) {
            // Client is giving us back a surface that it's since requested but hasn't gotten yet.
            // Do nothing.  It will be notified when the new surface is ready, and it can call us
            // again for the other surface, if it wants.
            return;
        }

        // Start destruction of this surface.  To prevent recursive call-backs to the client, we
        // post this for later.
        detachSurfaceLater(unowned);
    }

    @Override
    public void recreateSurface() {
        // If they don't have a surface, then they'll get a new one anyway.
        if (mOwnedByClient == null) return;

        // Notify the client that it no longer owns this surface, then destroy it.  When destruction
        // completes, we will recreate it automatically, since it will look like the client since
        // re-requested it.  That's why we send surfaceDestroyed here rather than letting our
        // surfaceDestroyed do it when destruction completes.  If we just started destruction while
        // the client still owns the surface, then our surfaceDestroyed would assume that Android
        // initiated the destruction, and wait for Android to recreate it.

        mParentView.post(new Runnable() {
            @Override
            public void run() {
                if (mOwnedByClient == null) return;
                SurfaceState owned = mOwnedByClient;
                mClient.surfaceDestroyed(owned.surfaceHolder().getSurface());
                mOwnedByClient = null;
                detachSurfaceNow(owned);
            }
        });
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        SurfaceState state = getStateForHolder(holder);
        assert state != null;

        // If this is the surface that the client currently cares about, then notify the client.
        // Note that surfaceChanged is guaranteed to come only after surfaceCreated.  Also, if the
        // client has requested a different surface but hasn't gotten it yet, then skip this.
        if (state == mOwnedByClient && state == mRequestedByClient) {
            state.width = width;
            state.height = height;
            state.format = format;
            mClient.surfaceChanged(holder.getSurface(), format, width, height);
        }
    }

    @Override
    public void surfaceRedrawNeeded(SurfaceHolder holder) {
        // Intentionally not implemented.
    }

    @Override
    public void surfaceRedrawNeededAsync(SurfaceHolder holder, Runnable drawingFinished) {
        mClient.surfaceRedrawNeededAsync(drawingFinished);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        SurfaceState state = getStateForHolder(holder);
        assert state != null;
        // Note that |createPending| might not be set, if Android destroyed and recreated this
        // surface on its own.

        Log.e(TAG, "surfaceCreated format : " + state.format);
        if (state != mRequestedByClient) {
            // Surface is created, but it's not the one that's been requested most recently.  Just
            // destroy it again.
            detachSurfaceLater(state);
            return;
        }

        // No create is pending.
        state.createPending = false;

        // A surfaceChanged should arrive.
        state.format = PixelFormat.UNKNOWN;

        // The client requested a surface, and it's now available.  If the client owns a surface,
        // then notify it that it doesn't.  Note that the client can't own |state| at this point,
        // since we would have removed ownership when we got surfaceDestroyed.  It's okay if the
        // client doesn't own either surface.
        assert mOwnedByClient != state;
        disownClientSurface(mOwnedByClient);

        // The client now owns this surface, so notify it.
        mOwnedByClient = mRequestedByClient;
        mClient.surfaceCreated(mOwnedByClient.surfaceHolder().getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        SurfaceState state = getStateForHolder(holder);
        assert state != null;

        // If no destroy is pending, then Android chose to destroy this surface and will, hopefully,
        // re-create it at some point.  Otherwise, a destroy is either posted or has already
        // detached this SurfaceView.  If it's already detached, then the destruction is complete
        // and we can clear |destroyPending|.  Otherwise, Android has destroyed this surface while
        // our destroy was posted, and might even return it before it runs.  When the post runs, it
        // can sort that out based on whether the surface is valid or not.
        Log.e(TAG, "surfaceDestroyed format : " + state.format);
        if (!state.destroyPending) {
            state.createPending = true;
        } else if (!state.isAttached()) {
            state.destroyPending = false;
        }

        state.format = PixelFormat.UNKNOWN;

        // If the client owns this surface, then notify it synchronously that it no longer does.
        // This can happen if Android destroys the surface on its own.  It's also possible that
        // we've detached it, if a destroy was pending.  Either way, notify the client.
        if (state == mOwnedByClient) {
            disownClientSurface(mOwnedByClient);

            // Do not re-request the surface here.  If android gives the surface back, then we'll
            // re-signal the client about construction.
            return;
        }

        // Make sure the client has no remaining references to the destroyed surface.
        mClient.unownedSurfaceDestroyed();

        // The client doesn't own this surface, but might want it.
        // If the client has requested this surface, then start construction on it.  The client will
        // be notified when it completes.  This can happen if the client re-requests a surface after
        // we start destruction on it from a previous request, for example.  We post this for later,
        // since we might be called while removing |state| from the view tree. In general, posting
        // from here is good.
        if (state == mRequestedByClient && !state.isAttached()) {
            attachSurfaceLater(state);
        } else if (state != mRequestedByClient && state.isAttached()) {
            // This isn't the requested surface.  If android destroyed it, then also unhook it so
            // that it isn't recreated later.
            detachSurfaceLater(state);
        }
    }

    @Override
    public void setBackgroundDrawable(Drawable background) {
        mTranslucent.surfaceView.setBackgroundDrawable(background);
        mOpaque.surfaceView.setBackgroundDrawable(background);
    }

    @Override
    public void setWillNotDraw(boolean willNotDraw) {
        mTranslucent.surfaceView.setWillNotDraw(willNotDraw);
        mOpaque.surfaceView.setWillNotDraw(willNotDraw);
    }

    @Override
    public void setVisibility(int visibility) {
        mTranslucent.surfaceView.setVisibility(visibility);
        mOpaque.surfaceView.setVisibility(visibility);
    }

    @Override
    public View getActiveSurfaceView() {
        return mOwnedByClient == null ? null : mOwnedByClient.surfaceView;
    }

    /**
     * Return the SurfaceState for |holder|, or null if it isn't either.
     */
    private SurfaceState getStateForHolder(SurfaceHolder holder) {
        if (mTranslucent.surfaceHolder() == holder) return mTranslucent;

        if (mOpaque.surfaceHolder() == holder) return mOpaque;

        return null;
    }

    /**
     * Attach |state| to |mParentView| immedaitely.
     */
    private void attachSurfaceNow(SurfaceState state) {
        if (state.isAttached()) return;

        // If there is a destroy in-flight for this surface, then do nothing.
        if (state.destroyPending) return;

        state.createPending = true;
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        state.attachTo(mParentView, lp);
        mParentView.bringChildToFront(state.surfaceView);
        mParentView.postInvalidateOnAnimation();
    }

    /**
     * Post a Runnable to attach |state|.  This is helpful, since one cannot directly interact with
     * the View heirarchy during Surface callbacks.
     */
    private void attachSurfaceLater(final SurfaceState state) {
        // We shouldn't try to post construction if there's an in-flight destroy.
        assert !state.destroyPending;
        state.createPending = true;

        mParentView.post(new Runnable() {
            @Override
            public void run() {
                attachSurfaceNow(state);
            }
        });
    }

    /**
     * Cause the client to disown |state| if it currently owns it.  This involves notifying it that
     * the surface has been destroyed (recall that ownership involves getting created).  It's okay
     * if |state| is null or isn't owned by the client.
     */
    private void disownClientSurface(SurfaceState state) {
        if (mOwnedByClient != state || state == null) return;

        mClient.surfaceDestroyed(mOwnedByClient.surfaceHolder().getSurface());
        mOwnedByClient = null;
    }

    /**
     * Detach |state| from |mParentView| immediately.
     */
    private void detachSurfaceNow(SurfaceState state) {
        // If we're called while we're not attached, then do nothing.  This makes it easier for the
        // client, since it doesn't have to keep track of whether the outgoing surface has been
        // destroyed or not.  The client will be notified (or has already) when the surface is
        // destroyed, if it currently owns it.
        if (state.isAttached()) {
            // We are attached.  If the surface is not valid, then Android has destroyed it for some
            // other reason, and we should clean up.  Otherwise, just wait for Android to finish.
            final boolean valid = state.isValid();

            // If the surface is valid, then we expect a callback to surfaceDestroyed eventually.
            state.destroyPending = valid;

            // Note that this might call back surfaceDestroyed before returning!
            state.detachFromParent();

            // If the surface was valid before, then we expect a surfaceDestroyed callback, which
            // might have arrived during removeView.  Either way, that callback will finish cleanup
            // of |state|.
            if (valid) return;
        }

        // The surface isn't attached, or was attached but wasn't currently valid.  Either way,
        // we're not going to get a destroy, so notify the client now if needed.
        disownClientSurface(state);

        // If the client has since re-requested the surface, then start construction.
        if (state == mRequestedByClient) attachSurfaceNow(mRequestedByClient);
    }

    /**
     * Post detachment of |state|.  This is safe during Surface callbacks.
     */
    private void detachSurfaceLater(final SurfaceState state) {
        // If |state| is not attached, then do nothing.  There might be a destroy pending from
        // Android, but in any case leave it be.
        if (!state.isAttached()) return;

        state.destroyPending = true;
        mParentView.post(new Runnable() {
            @Override
            public void run() {
                detachSurfaceNow(state);
            }
        });
    }
}
