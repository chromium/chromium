// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.gfx.AwDrawFnImpl.DrawFnAccess;
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

    public boolean isInitialized() {
        return mAwContents != null;
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
