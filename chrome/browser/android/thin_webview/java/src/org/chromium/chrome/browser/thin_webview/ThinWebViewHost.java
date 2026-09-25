// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thin_webview;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Helper to bind a WebContents to a ThinWebView for display in a container.
 *
 * <p>This class does not offer any lifecycle guarantees and can be used in features with different
 * lifecycle expectations (e.g., tab-scoped or window-scoped) as long as an instance of this class
 * does not outlive its {@link WindowAndroid}. The caller is fully responsible for creating and
 * destroying (via {@link #destroy()}) an instance of this class in accordance with the caller's
 * intended lifecycle.
 */
@JNINamespace("thin_webview::android")
@NullMarked
public final class ThinWebViewHost {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ThinWebView mThinWebView;
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    // An instance is owned either by Java ({@link #create}) or by the native TabThinWebViewHost
    // ({@link #createNativeOwned}). A natively owned instance mirrors state that native also
    // tracks. This flag is used to enforce the constraint.
    private final boolean mIsNativeOwned;

    private @Nullable WebContents mWebContents;

    private ThinWebViewHost(WindowAndroid windowAndroid, boolean isNativeOwned) {
        Context context = windowAndroid.getContext().get();
        assert context != null;
        mContext = context;
        mWindowAndroid = windowAndroid;
        mIsNativeOwned = isNativeOwned;
        mThinWebView = createThinWebView(context, windowAndroid);
    }

    private static ThinWebView createThinWebView(Context context, WindowAndroid windowAndroid) {
        var intentRequestTracker = windowAndroid.getIntentRequestTracker();
        assert intentRequestTracker != null;

        // We can't use `windowAndroid` to create the ThinWebView as that WindowAndroid already has
        // a compositor.
        return ThinWebViewFactory.create(
                context,
                new ThinWebViewConstraints(),
                intentRequestTracker,
                /* enablePermissionRequests= */ false);
    }

    /**
     * Creates a Java-owned instance bound to the given {@link WindowAndroid}.
     *
     * <p>The binding is fixed for the host's lifetime; create a new host to show a different {@link
     * WebContents}.
     *
     * @param webContents The {@link WebContents} to attach.
     * @param windowAndroid The window hosting the view.
     * @return The new host, which the caller must {@link #destroy()}.
     */
    public static ThinWebViewHost create(WebContents webContents, WindowAndroid windowAndroid) {
        ThinWebViewHost host = new ThinWebViewHost(windowAndroid, /* isNativeOwned= */ false);
        host.setWebContents(webContents);
        return host;
    }

    @CalledByNative
    private static ThinWebViewHost createNativeOwned(
            @JniType("content::WebContents*") WebContents webContents,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid) {
        ThinWebViewHost host = new ThinWebViewHost(windowAndroid, /* isNativeOwned= */ true);
        host.setWebContents(webContents);
        return host;
    }

    /**
     * Binds a {@link WebContents} to the host's {@link ThinWebView}.
     *
     * <p>This sets up the Android view delegates and attaches the WebContents to the underlying
     * ThinWebView. Note that this intentionally does not set a WebContentsDelegateAndroid: doing so
     * would overwrite any delegate the owner has installed from C++.
     *
     * <p>Private because a natively owned host keeps its bound WebContents in sync with native's
     * own copy; rebinding is driven from native.
     */
    @CalledByNative
    private void setWebContents(@JniType("content::WebContents*") WebContents webContents) {
        LifetimeAssert.assertNotDestroyed(mLifetimeAssert);
        WebContents prevWebContents = mWebContents;
        if (prevWebContents == webContents) return;

        if (prevWebContents != null && !prevWebContents.isDestroyed()) {
            prevWebContents.setTopLevelNativeWindow(null);
        }

        mWebContents = webContents;

        ContentView contentView = ContentView.createContentView(mContext, webContents);
        contentView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        ViewAndroidDelegate viewDelegate = webContents.getViewAndroidDelegate();
        if (viewDelegate == null) {
            webContents.setDelegates(
                    VersionInfo.getProductVersion(),
                    ViewAndroidDelegate.createBasicDelegate(contentView),
                    contentView,
                    mWindowAndroid,
                    WebContents.createDefaultInternalsHolder());
        } else {
            // Most systems assume the ViewAndroidDelegate is created alongside the WebContents and
            // never changes (SelectionPopupControllerImpl, for one), so we must reuse the existing
            // delegate rather than call setDelegates() again. Instead, mirror the internal updates
            // setDelegates() would have made for the things that actually changed.
            //
            // Note that setTopLevelNativeWindow() does not durably re-parent the WebContents:
            // attachWebContents() below re-parents its ViewAndroid under the ThinWebView's own
            // WindowAndroid, so getTopLevelNativeWindow() will not return `mWindowAndroid`. The
            // call is made solely for its side effect of pointing the WebContents'
            // ColorProviderSource at the host activity's window, which (unlike the ThinWebView's
            // orphaned window) has the real theme state.
            webContents.setTopLevelNativeWindow(mWindowAndroid);
            viewDelegate.setContainerView(contentView);
            ViewEventSink.from(webContents).setAccessDelegate(contentView);
        }

        mThinWebView.attachWebContents(
                webContents, contentView, new ThinWebViewAttachParams.Builder().build());
    }

    /**
     * Returns the root Android View of the ThinWebView to be embedded in the container.
     *
     * <p>Must not be called after the host has been destroyed.
     */
    @CalledByNative
    public View getView() {
        LifetimeAssert.assertNotDestroyed(mLifetimeAssert);
        return mThinWebView.getView();
    }

    /**
     * Destroys the ThinWebView, releasing its surface resources. The host cannot be used
     * afterwards.
     *
     * <p>The bound {@link WebContents} is not destroyed: it is owned by the caller and may outlive
     * this host (e.g. it travels with a reparented tab).
     */
    public void destroy() {
        assert !mIsNativeOwned
                : "Natively owned ThinWebViewHost must be destroyed by native "
                        + "TabThinWebViewHost, not from Java.";
        destroyInternal();
    }

    @CalledByNative
    private void destroyInternal() {
        LifetimeAssert.destroy(mLifetimeAssert);
        // The WebContents is intentionally left bound to this host's WindowAndroid rather than
        // being hidden and unparented here. Every caller either destroys the WebContents right
        // after this, or is reparenting a tab, in which case the destination window shows the
        // panel immediately and re-binds via setWebContents(). Destroying the ThinWebView also
        // destroys the WindowAndroid it owns, which detaches the ViewAndroid regardless.
        mWebContents = null;
        mThinWebView.destroy();
    }
}
