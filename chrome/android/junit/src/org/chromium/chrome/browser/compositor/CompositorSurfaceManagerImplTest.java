// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.PixelFormat;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSurfaceView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.Set;

/** Unit tests for the CompositorSurfaceManagerImpl. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class CompositorSurfaceManagerImplTest {
    @Mock private CompositorSurfaceManager.SurfaceManagerCallbackTarget mCallback;

    private CompositorSurfaceManager mManager;

    private FrameLayout mLayout;

    // surfaceChanged parameters chosen by most recent sendSurfaceChanged.
    private int mActualFormat;
    private int mWidth;
    private int mHeight;

    /**
     * Implementation of a SurfaceView shadow that provides additional functionality for controlling
     * the state of the underlying (fake) Surface.
     */
    @Implements(SurfaceView.class)
    public static class MyShadowSurfaceView extends ShadowSurfaceView {
        private final MyFakeSurfaceHolder mHolder = new MyFakeSurfaceHolder();

        /** Robolectric's FakeSurfaceHolder doesn't keep track of the format, etc. */
        public static class MyFakeSurfaceHolder extends ShadowSurfaceView.FakeSurfaceHolder {
            /** Fake surface that lets us control whether it's valid or not. */
            public static class MyFakeSurface extends Surface {
                public boolean valid;

                @Override
                public boolean isValid() {
                    return valid;
                }
            }

            private int mFormat = PixelFormat.UNKNOWN;
            private final MyFakeSurface mSurface = new MyFakeSurface();

            @Override
            @Implementation
            public void setFormat(int format) {
                mFormat = format;
            }

            public int getFormat() {
                return mFormat;
            }

            // Return a surface that we can control if it's valid or not.
            @Override
            public Surface getSurface() {
                return getFakeSurface();
            }

            public MyFakeSurface getFakeSurface() {
                return mSurface;
            }
        }

        public MyShadowSurfaceView() {}

        @Override
        @Implementation
        public SurfaceHolder getHolder() {
            return getMyFakeSurfaceHolder();
        }

        @Override
        public FakeSurfaceHolder getFakeSurfaceHolder() {
            return getMyFakeSurfaceHolder();
        }

        public MyFakeSurfaceHolder getMyFakeSurfaceHolder() {
            return mHolder;
        }
    }

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mLayout = new FrameLayout(activity);
        mManager = new CompositorSurfaceManagerImpl(mLayout, mCallback);
    }

    private void runDelayedTasks() {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    /** Return the callback for |view|, or null.  Will get mad if there's more than one. */
    private SurfaceHolder.Callback callbackFor(SurfaceView view) {
        MyShadowSurfaceView viewShadow = (MyShadowSurfaceView) Shadows.shadowOf(view);
        ShadowSurfaceView.FakeSurfaceHolder viewHolder = viewShadow.getFakeSurfaceHolder();
        Set<SurfaceHolder.Callback> callbacks = viewHolder.getCallbacks();
        // Zero or one is okay.
        assertThat(callbacks.size(), lessThan(2));

        if (callbacks.size() == 1) return callbacks.iterator().next();

        return null;
    }

    private MyShadowSurfaceView.MyFakeSurfaceHolder fakeHolderFor(SurfaceView view) {
        MyShadowSurfaceView viewShadow = (MyShadowSurfaceView) Shadows.shadowOf(view);
        return viewShadow.getMyFakeSurfaceHolder();
    }

    private void setSurfaceValid(SurfaceView view, boolean valid) {
        fakeHolderFor(view).getFakeSurface().valid = valid;
    }

    /** Find and return the SurfaceView with format |format|. */
    private SurfaceView findSurface(int format) {
        final int childCount = mLayout.getChildCount();
        for (int i = 0; i < childCount; i++) {
            final SurfaceView child = (SurfaceView) mLayout.getChildAt(i);
            if (fakeHolderFor(child).getFormat() == format) return child;
        }

        return null;
    }

    /**
     * Request the pixel format |format|, and return the SurfaceView for it if it's attached.  You
     * are responsible for sending surfaceCreated / Changed to |mManager| if you want it to think
     * that Android has provided the Surface.
     */
    private SurfaceView requestSurface(int format) {
        mManager.requestSurface(format);
        runDelayedTasks();

        return findSurface(format);
    }

    /**
     * Request format |format|, and send created / changed callbacks to |mManager| as if Android
     * had provided the underlying Surface.
     */
    private SurfaceView requestThenCreateSurface(int format) {
        SurfaceView view = requestSurface(format);
        setSurfaceValid(view, true);
        callbackFor(view).surfaceCreated(view.getHolder());
        sendSurfaceChanged(view, format, 320, 240);

        return view;
    }

    /** Send a surfaceChanged event with the given parameters. */
    private void sendSurfaceChanged(SurfaceView view, int format, int width, int height) {
        mActualFormat =
                (format == PixelFormat.OPAQUE) ? PixelFormat.RGB_565 : PixelFormat.RGBA_8888;
        mWidth = width;
        mHeight = height;
        callbackFor(view).surfaceChanged(view.getHolder(), mActualFormat, mWidth, mHeight);
    }

    @Test
    @Feature("Compositor")
    @Config(shadows = {MyShadowSurfaceView.class})
    public void testRequestOpaqueSurface() {
        // Request a SurfaceView, and test in detail that it worked.
        SurfaceView opaque = requestSurface(PixelFormat.OPAQUE);
        verify(mCallback, times(0)).surfaceCreated(ArgumentMatchers.<Surface>any());
        verify(mCallback, times(0))
                .surfaceChanged(ArgumentMatchers.<Surface>any(), anyInt(), anyInt(), anyInt());
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());

        // Check that there's an opaque SurfaceView .
        assertEquals(1, mLayout.getChildCount());
        assertTrue(fakeHolderFor(opaque).getFormat() == PixelFormat.OPAQUE);

        // Verify that we are notified when the surface is created.
        callbackFor(opaque).surfaceCreated(opaque.getHolder());
        verify(mCallback, times(1)).surfaceCreated(eq(opaque.getHolder().getSurface()));
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());

        // Verify that we are notified when the surface is changed.
        sendSurfaceChanged(opaque, PixelFormat.OPAQUE, 320, 240);
        verify(mCallback, times(1)).surfaceCreated(eq(opaque.getHolder().getSurface()));
        verify(mCallback, times(1))
                .surfaceChanged(
                        eq(opaque.getHolder().getSurface()),
                        eq(mActualFormat),
                        eq(mWidth),
                        eq(mHeight));
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());

        // Verify that we are notified when the surface is destroyed.
        callbackFor(opaque).surfaceDestroyed(opaque.getHolder());
        verify(mCallback, times(1)).surfaceCreated(eq(opaque.getHolder().getSurface()));
        verify(mCallback, times(1))
                .surfaceChanged(eq(opaque.getHolder().getSurface()), anyInt(), anyInt(), anyInt());
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), true);
    }

    @Test
    @Feature("Compositor")
    @Config(shadows = {MyShadowSurfaceView.class})
    public void testRequestOpaqueThenTranslucentSurface() {
        // Request opaque then translucent.
        SurfaceView opaque = requestThenCreateSurface(PixelFormat.OPAQUE);
        SurfaceView translucent = requestThenCreateSurface(PixelFormat.TRANSLUCENT);

        // Verify that we received a destroy for |opaque| and created / changed for |translucent|.
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), false);
        verify(mCallback, times(1)).surfaceCreated(translucent.getHolder().getSurface());
        verify(mCallback, times(1))
                .surfaceChanged(
                        eq(translucent.getHolder().getSurface()), anyInt(), anyInt(), anyInt());

        // Both views should be present.
        assertEquals(2, mLayout.getChildCount());

        // Only the translucent surface should be left.  Note that the old view is still valid.
        mManager.doneWithUnownedSurface();
        runDelayedTasks();
        assertEquals(1, mLayout.getChildCount());
        assertNotNull(findSurface(PixelFormat.TRANSLUCENT));
    }

    @Test
    @Feature("Compositor")
    @Config(shadows = {MyShadowSurfaceView.class})
    public void testRequestSameSurface() {
        // Request an opaque surface, get it, then request it again.  Verify that we get synthetic
        // create / destroy callbacks.
        SurfaceView opaque = requestThenCreateSurface(PixelFormat.OPAQUE);
        verify(mCallback, times(1)).surfaceCreated(eq(opaque.getHolder().getSurface()));
        verify(mCallback, times(1))
                .surfaceChanged(eq(opaque.getHolder().getSurface()), anyInt(), anyInt(), anyInt());
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());

        // Surface is currently valid.  Request again.  We should get back a destroy and create.
        assertEquals(opaque, requestSurface(PixelFormat.OPAQUE));
        verify(mCallback, times(2)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(2))
                .surfaceChanged(
                        eq(opaque.getHolder().getSurface()),
                        eq(mActualFormat),
                        eq(mWidth),
                        eq(mHeight));
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), false);
        assertEquals(1, mLayout.getChildCount());
    }

    @Test
    @Feature("Compositor")
    @Config(shadows = {MyShadowSurfaceView.class})
    public void testRequestSameSurfaceBeforeReady() {
        // Request an opaque surface, then request it again before the first one shows up.
        SurfaceView opaque = requestSurface(PixelFormat.OPAQUE);
        verify(mCallback, times(0)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(0))
                .surfaceChanged(eq(opaque.getHolder().getSurface()), anyInt(), anyInt(), anyInt());
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());

        // Request again.  We shouldn't get any callbacks, since the surface is still pending.
        assertEquals(opaque, requestSurface(PixelFormat.OPAQUE));
        verify(mCallback, times(0)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(0))
                .surfaceChanged(eq(opaque.getHolder().getSurface()), anyInt(), anyInt(), anyInt());
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());

        // Only the opaque view should be attached.
        assertEquals(1, mLayout.getChildCount());

        // When the surface is created, we should get notified created / changed, but not destroyed.
        callbackFor(opaque).surfaceCreated(opaque.getHolder());
        verify(mCallback, times(1)).surfaceCreated(opaque.getHolder().getSurface());

        sendSurfaceChanged(opaque, PixelFormat.RGB_565, 320, 240);
        verify(mCallback, times(1))
                .surfaceChanged(
                        eq(opaque.getHolder().getSurface()),
                        eq(mActualFormat),
                        eq(mWidth),
                        eq(mHeight));
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());
    }

    @Test
    @Feature("Compositor")
    @Config(shadows = {MyShadowSurfaceView.class})
    public void testRequestDifferentSurfacesBeforeReady() {
        // Request an opaque surface, then request the translucent one before the it one shows up.
        SurfaceView opaque = requestSurface(PixelFormat.OPAQUE);
        verify(mCallback, times(0)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(0))
                .surfaceChanged(eq(opaque.getHolder().getSurface()), anyInt(), anyInt(), anyInt());
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());

        // Request translucent.  We should get no callbacks, but both views should be attached.
        SurfaceView translucent = requestSurface(PixelFormat.TRANSLUCENT);
        verify(mCallback, times(0)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(0)).surfaceCreated(translucent.getHolder().getSurface());
        assertEquals(2, mLayout.getChildCount());

        // If the opaque surface arrives, we shouldn't hear about it.  It should be detached, since
        // we've requested the other one.
        callbackFor(opaque).surfaceCreated(opaque.getHolder());
        runDelayedTasks();
        assertEquals(1, mLayout.getChildCount());
        verify(mCallback, times(0)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(0)).surfaceCreated(translucent.getHolder().getSurface());
        verify(mCallback, times(0)).surfaceDestroyed(any(), anyBoolean());

        // When we create the translucent surface, we should be notified.
        callbackFor(translucent).surfaceCreated(translucent.getHolder());
        verify(mCallback, times(0)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(1)).surfaceCreated(translucent.getHolder().getSurface());
    }

    @Test
    @Feature("Compositor")
    @Config(shadows = {MyShadowSurfaceView.class})
    public void testPendingSurfaceChangedCallback() {
        // Request an opaque surface, and request it again between 'created' and 'changed'.  We
        // should get a synthetic 'created', but a real 'changed' callback.
        SurfaceView opaque = requestSurface(PixelFormat.OPAQUE);
        callbackFor(opaque).surfaceCreated(opaque.getHolder());
        runDelayedTasks();

        // Sanity check.
        verify(mCallback, times(1)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(0))
                .surfaceChanged(eq(opaque.getHolder().getSurface()), anyInt(), anyInt(), anyInt());

        // Re-request while 'changed' is still pending.  We should get a synthetic 'destroyed' and
        // synthetic 'created'.
        assertEquals(opaque, requestSurface(PixelFormat.OPAQUE));
        verify(mCallback, times(2)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), false);
        verify(mCallback, times(0))
                .surfaceChanged(eq(opaque.getHolder().getSurface()), anyInt(), anyInt(), anyInt());

        // Send 'changed', and expect that we'll receive it.
        sendSurfaceChanged(opaque, PixelFormat.OPAQUE, 320, 240);
        verify(mCallback, times(1))
                .surfaceChanged(
                        eq(opaque.getHolder().getSurface()),
                        eq(mActualFormat),
                        eq(mWidth),
                        eq(mHeight));
    }

    @Test
    @Feature("Compositor")
    @Config(shadows = {MyShadowSurfaceView.class})
    public void testRecreateSurface() {
        // See if recreateSurface destroys / re-creates the surface.
        // should get a synthetic 'created', but a real 'changed' callback.
        SurfaceView opaque = requestThenCreateSurface(PixelFormat.OPAQUE);
        verify(mCallback, times(1)).surfaceCreated(opaque.getHolder().getSurface());
        assertEquals(1, mLayout.getChildCount());

        // We should be notified that the surface was destroyed via synthetic callback, and the
        // surface should be detached.
        mManager.recreateSurface();
        verify(mCallback, times(1)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), true);
        assertEquals(0, mLayout.getChildCount());

        // When the surface really is destroyed, it should be re-attached.  We should not be
        // notified again, though.
        callbackFor(opaque).surfaceDestroyed(opaque.getHolder());
        verify(mCallback, times(1)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), true);
        assertEquals(1, mLayout.getChildCount());

        // When the surface is re-created, we should be notified.
        callbackFor(opaque).surfaceCreated(opaque.getHolder());
        verify(mCallback, times(2)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), true);
        assertEquals(1, mLayout.getChildCount());
    }

    @Test
    @Feature("Compositor")
    @Config(shadows = {MyShadowSurfaceView.class})
    public void testRequestSurfaceDuringDestruction() {
        // If we re-request a surface while we're tearing it down, it should be re-attached and
        // given back to us once the destruction completes.
        SurfaceView opaque = requestThenCreateSurface(PixelFormat.OPAQUE);
        SurfaceView translucent = requestThenCreateSurface(PixelFormat.TRANSLUCENT);
        mManager.doneWithUnownedSurface();

        // The transparent surface should be attached, and the opaque one detached.
        assertEquals(1, mLayout.getChildCount());
        assertNotNull(findSurface(PixelFormat.TRANSLUCENT));

        // Re-request the opaque surface.  Nothing should happen until it's destroyed.  It should
        // not be re-attached, since that is also deferred until destruction.
        assertEquals(null, requestSurface(PixelFormat.OPAQUE));
        assertEquals(1, mLayout.getChildCount());
        assertNotNull(findSurface(PixelFormat.TRANSLUCENT));

        // When the opaque surface is destroyed, then it should be re-attached.  No callbacks should
        // have arrived yet, except for initial creation and (synthetic) destroyed when we got the
        // translucent surface.
        callbackFor(opaque).surfaceDestroyed(opaque.getHolder());
        assertEquals(2, mLayout.getChildCount());
        verify(mCallback, times(1)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), false);
        verify(mCallback, times(0)).surfaceDestroyed(translucent.getHolder().getSurface(), true);

        // When the opaque surface becomes available, we'll get the synthetic destroy for the
        // translucent one that we lost ownership of, and the real create for the opaque one.
        callbackFor(opaque).surfaceCreated(opaque.getHolder());
        assertEquals(2, mLayout.getChildCount());
        verify(mCallback, times(2)).surfaceCreated(opaque.getHolder().getSurface());
        verify(mCallback, times(1)).surfaceDestroyed(translucent.getHolder().getSurface(), false);
        verify(mCallback, times(1)).surfaceDestroyed(opaque.getHolder().getSurface(), false);
    }
}
