// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.gfx.AwDrawFnImpl.DrawFnAccess;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Represents underlying Chromium web contents state that can survive moving across WebViews. */
@NullMarked
public class AwWebContent {
    /**
     * Listener notified when the {@link AwContents} associated with this {@link AwWebContent}
     * changes, or {@code null} when detached or not yet initialized.
     */
    public interface SurfaceBindingListener {
        void onAwContentsChanged(@Nullable AwContents awContents);
    }

    /** The current owner of this {@link AwWebContent}. There is at most one host at a time. */
    public interface ViewHost {
        /**
         * Called when `AwSettings` is available during adoption.
         *
         * <p>`AwContents` owns the `AwSettings` object so in an ideal world, we shouldn't be
         * passing this to the host. But we observed that during `AwContents` construction,
         * depending on the app's implementation, it is possible that there is a call to
         * `WebViewChromium.getSettings()`. Since `AwContents` is not fully initialized at that
         * point, `getSettings()` would return `null`. Therefore, we pass the settings to the host
         * as soon as the settings are available. Refer: b/556721003
         */
        void initSettings(AwSettings settings);

        /**
         * Called when this {@link AwWebContent} is adopted by a new host, before the new host's
         * adoption begins.
         */
        void onDetached();
    }

    private @Nullable AwContents mAwContents;
    private @Nullable ViewHost mCurrentHost;
    private @Nullable SurfaceBindingListener mSurfaceBindingListener;
    private boolean mIsDestroyed;

    /**
     * Creates an {@link AwWebContent} that has already been destroyed.
     *
     * <p>A host whose content has been adopted by another host can swap its {@link AwWebContent} to
     * the returned instance so that any further calls on the old host are ignored, the same way
     * calls on a destroyed WebView are.
     *
     * <p>The returned instance cannot not be adopted.
     *
     * <p>Each call creates a new destroyed {@link AwContents}.
     *
     * <p>Must only be called on the UI thread after Chromium startup has completed.
     *
     * <p>TODO(bewise): Replace this destroyed AwWebContent approach with better state management so
     * that long term we can clean up the default profile when it isn't in use.
     */
    public static AwWebContent createDestroyedAwWebContent(
            AwContents.AwContentsClientFactory clientFactory) {
        ThreadUtils.assertOnUiThread();
        Context appContext = ContextUtils.getApplicationContext();
        AwWebContent destroyedAwWebContent = new AwWebContent();
        destroyedAwWebContent.mAwContents =
                new AwContents(
                        AwBrowserContextStore.getNamedContext(
                                AwBrowserContext.getDefaultContextName(), true),
                        new FrameLayout(appContext),
                        appContext,
                        new NullInternalAccessDelegate(),
                        (canvas, functor) -> {},
                        clientFactory);
        destroyedAwWebContent.mAwContents.destroy();
        destroyedAwWebContent.mIsDestroyed = true;
        return destroyedAwWebContent;
    }

    /** No-op {@link InternalAccessDelegate} for the destroyed {@link AwContents}. */
    private static final class NullInternalAccessDelegate implements InternalAccessDelegate {
        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return false;
        }

        @Override
        public void super_onConfigurationChanged(Configuration newConfig) {}

        @Override
        public int super_getScrollBarStyle() {
            return View.SCROLLBARS_INSIDE_OVERLAY;
        }

        @Override
        public void super_startActivityForResult(Intent intent, int requestCode) {}

        @Override
        public void onScrollChanged(int l, int t, int oldl, int oldt) {}

        @Override
        public void overScrollBy(
                int deltaX,
                int deltaY,
                int scrollX,
                int scrollY,
                int scrollRangeX,
                int scrollRangeY,
                int maxOverScrollX,
                int maxOverScrollY,
                boolean isTouchEvent) {}

        @Override
        public void super_scrollTo(int scrollX, int scrollY) {}

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {}
    }

    public boolean isInitialized() {
        return mAwContents != null;
    }

    /**
     * Returns the {@link AwContents} backing this {@link AwWebContent}, or null if it has not been
     * adopted yet.
     *
     * <p>TODO: Eventually get rid of this method such that holders hold onto `AwWebContent` instead
     * of `AwContents`.
     */
    public @Nullable AwContents getAwContents() {
        return mAwContents;
    }

    public void bindSurface(@Nullable SurfaceBindingListener listener) {
        if (mSurfaceBindingListener == listener) {
            return;
        }
        if (mSurfaceBindingListener != null) {
            mSurfaceBindingListener.onAwContentsChanged(null);
        }
        mSurfaceBindingListener = listener;
        if (mSurfaceBindingListener != null) {
            mSurfaceBindingListener.onAwContentsChanged(mAwContents);
        }
    }

    public AwContents adopt(
            ViewHost host,
            AwBrowserContext browserContext,
            ViewGroup containerView,
            Context context,
            InternalAccessDelegate internalAccessAdapter,
            DrawFnAccess drawFnAccess,
            AwContents.AwContentsClientFactory clientFactory,
            DependencyFactory dependencyFactory) {
        if (mIsDestroyed) {
            throw new IllegalStateException(
                    "Cannot adopt an AwWebContent instance after destroy() has been called.");
        }

        assert mCurrentHost != host : "Cannot adopt an AwWebContent into the same host twice.";

        if (mCurrentHost != null) {
            mCurrentHost.onDetached();
        }
        mCurrentHost = host;

        if (mAwContents == null) {
            mAwContents =
                    new AwContents(
                            browserContext,
                            containerView,
                            context,
                            internalAccessAdapter,
                            drawFnAccess,
                            clientFactory,
                            host::initSettings,
                            dependencyFactory);
        } else {
            host.initSettings(mAwContents.getSettings());
            mAwContents.adopt(containerView, internalAccessAdapter);
        }

        if (mSurfaceBindingListener != null) {
            mSurfaceBindingListener.onAwContentsChanged(mAwContents);
        }

        return mAwContents;
    }

    public void destroy() {
        if (mIsDestroyed) return;
        mIsDestroyed = true;
        if (mSurfaceBindingListener != null) {
            mSurfaceBindingListener.onAwContentsChanged(null);
            mSurfaceBindingListener = null;
        }
        mCurrentHost = null;
        if (mAwContents != null) {
            mAwContents.destroy();
            mAwContents = null;
        }
    }
}
