// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thinwebview.internal;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.thinwebview.CompositorView;
import org.chromium.chrome.browser.thinwebview.ThinWebView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * An android view backed by a {@link Surface} that is able to display a live {@link WebContents}.
 */
@JNINamespace("thin_webview::android")
public class ThinWebViewImpl extends FrameLayout implements ThinWebView {
    private final CompositorView mCompositorView;
    private long mNativeThinWebViewImpl;
    private WebContents mWebContents;
    private View mContentView;

    /**
     * Creates a {@link ThinWebViewImpl} backed by a {@link Surface}.
     * @param context The Context to create this view.
     * @param windowAndroid The associated {@code WindowAndroid} on which the view is to be
     *         displayed.
     */
    public ThinWebViewImpl(Context context, WindowAndroid windowAndroid) {
        super(context);
        mCompositorView = new CompositorViewImpl(context, windowAndroid);

        LayoutParams layoutParams = new LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        addView(mCompositorView.getView(), layoutParams);

        mNativeThinWebViewImpl =
                ThinWebViewImplJni.get().init(ThinWebViewImpl.this, mCompositorView, windowAndroid);
    }

    @Override
    public View getView() {
        return this;
    }

    @Override
    public void attachWebContents(WebContents webContents, @Nullable View contentView) {
        mWebContents = webContents;

        setContentView(contentView);
        ThinWebViewImplJni.get().setWebContents(
                mNativeThinWebViewImpl, ThinWebViewImpl.this, mWebContents);
        mWebContents.onShow();
    }

    @Override
    public void destroy() {
        mCompositorView.destroy();
        if (mNativeThinWebViewImpl != 0) {
            ThinWebViewImplJni.get().destroy(mNativeThinWebViewImpl, ThinWebViewImpl.this);
            mNativeThinWebViewImpl = 0;
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (w != oldw || h != oldh) {
            ThinWebViewImplJni.get().sizeChanged(
                    mNativeThinWebViewImpl, ThinWebViewImpl.this, w, h);
        }
    }

    private void setContentView(View contentView) {
        if (mContentView == contentView) return;

        if (mContentView != null) {
            assert getChildCount() > 1;
            removeViewAt(1);
        }

        mContentView = contentView;
        if (mContentView != null) addView(mContentView, 1);
    }

    @NativeMethods
    interface Natives {
        long init(
                ThinWebViewImpl caller, CompositorView compositorView, WindowAndroid windowAndroid);
        void destroy(long nativeThinWebView, ThinWebViewImpl caller);
        void setWebContents(
                long nativeThinWebView, ThinWebViewImpl caller, WebContents webContents);
        void sizeChanged(long nativeThinWebView, ThinWebViewImpl caller, int width, int height);
    }
}
