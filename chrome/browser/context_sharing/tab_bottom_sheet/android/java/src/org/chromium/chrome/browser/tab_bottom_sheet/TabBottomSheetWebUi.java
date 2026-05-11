// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.components.thinwebview.internal.ThinWebViewContextMenuItemDelegate;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/** Abstract class for Tab Bottom Sheet toolbars. */
@NullMarked
public class TabBottomSheetWebUi {
    private static boolean sInTestMode;

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ContextMenuPopulatorFactory mContextMenuPopulatorFactory;
    private final WebViewResizingHelper mWebViewResizingHelper;
    private final @ColorInt int mBackgroundColor;
    private final CoBrowseViewsZoomControl mZoomControl;

    private ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private @Nullable ContentView mContentView;

    TabBottomSheetWebUi(
            Context context,
            View containerView,
            WindowAndroid windowAndroid,
            ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @ColorInt int backgroundColor,
            CoBrowseViewsZoomControl zoomControl) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mContextMenuPopulatorFactory = contextMenuPopulatorFactory;
        mBackgroundColor = backgroundColor;
        mZoomControl = zoomControl;
        mWebViewResizingHelper =
                new WebViewResizingHelper(containerView, windowAndroid, backgroundColor);
        resetThinWebView();
    }

    @SuppressLint("ClickableViewAccessibility")
    void setWebContents(@Nullable WebContents webContents) {
        if (mWebContents == webContents) {
            return;
        }
        mWebContents = webContents;
        if (mWebContents != null) {
            mWebContents.getEventForwarder().setCurrentTouchOffsetX(0.0f);
            mWebContents.getEventForwarder().setCurrentTouchOffsetY(0.0f);
            // Use a local variable to ensure we are using the correct ContentView instance.
            ContentView contentView = createContentView(mContext, mWebContents);
            mContentView = contentView;

            contentView.addOnAttachStateChangeListener(
                    new View.OnAttachStateChangeListener() {
                        private final ViewTreeObserver.OnWindowFocusChangeListener mListener =
                                new ViewTreeObserver.OnWindowFocusChangeListener() {
                                    @Override
                                    public void onWindowFocusChanged(boolean hasFocus) {
                                        if (!hasFocus) {
                                            contentView.clearFocus();
                                        }
                                    }
                                };

                        @Override
                        public void onViewAttachedToWindow(View v) {
                            contentView
                                    .getViewTreeObserver()
                                    .addOnWindowFocusChangeListener(mListener);
                        }

                        @Override
                        public void onViewDetachedFromWindow(View v) {
                            contentView
                                    .getViewTreeObserver()
                                    .removeOnWindowFocusChangeListener(mListener);
                        }
                    });

            // Most systems assume ViewAndroidDelegate is created alongside WebContents and never
            // changes. SelectionPopupControllerImpl is an example of a system that does this so if
            // we don't reuse the existing delegate, popups will break.
            ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
            if (viewDelegate == null) {
                mWebContents.setDelegates(
                        VersionInfo.getProductVersion(),
                        ViewAndroidDelegate.createBasicDelegate(contentView),
                        contentView,
                        mWindowAndroid,
                        WebContents.createDefaultInternalsHolder());
            } else {
                // This mirrors the internal updates that happen in setDelegates for the things
                // that may have changed (contentView and WindowAndroid).
                mWebContents.setTopLevelNativeWindow(mWindowAndroid);
                viewDelegate.setContainerView(contentView);

                // Working with this in a test is impossible as ViewEventSinkImpl is final and
                // WebContentsImpl is not reachable to mock. As such, we need to skip this step in
                // test mode.
                if (!sInTestMode) {
                    ViewEventSink.from(mWebContents).setAccessDelegate(contentView);
                }
            }
            ThinWebViewContextMenuItemDelegate itemDelegate =
                    new ThinWebViewContextMenuItemDelegate(mWebContents);
            mContextMenuPopulatorFactory.setItemDelegate(itemDelegate);
            mThinWebView.attachWebContents(
                    mWebContents,
                    contentView,
                    new ThinWebViewAttachParams.Builder()
                            .setWebContentsDelegate(createWebContentsDelegate())
                            .setContextMenuPopulatorFactory(mContextMenuPopulatorFactory)
                            .setSupportTheming(true)
                            .build());
            mWebViewResizingHelper.setThinWebView(mThinWebView, mWebContents);
        } else {
            resetThinWebView();
        }
    }

    @Nullable WebContents getWebContents() {
        return mWebContents;
    }

    void setIgnoreClearFocus(boolean ignoreClearFocus) {
        if (mContentView != null) {
            mContentView.setIgnoreClearFocus(ignoreClearFocus);
        }
    }

    WebViewResizingHelper getWebViewResizingHelper() {
        return mWebViewResizingHelper;
    }

    void destroy() {
        // We expect the life cycle of webContents to be managed by native.
        mWebContents = null;
        mContentView = null;
        mWebViewResizingHelper.reset();
        mThinWebView.destroy();
    }

    View getWebUiView() {
        return mWebViewResizingHelper.getResizingContainer();
    }

    private WebContentsDelegateAndroid createWebContentsDelegate() {
        return new WebContentsDelegateAndroid() {
            @Override
            public void contentsZoomChange(boolean zoomIn) {
                if (mWebContents == null) return;
                if (zoomIn) {
                    mZoomControl.zoomIn(mWebContents);
                } else {
                    mZoomControl.zoomOut(mWebContents);
                }
            }
        };
    }

    @VisibleForTesting
    ContentView createContentView(Context context, WebContents webContents) {
        return ContentView.createContentView(context, webContents);
    }

    private void resetThinWebView() {
        if (mThinWebView != null) mThinWebView.destroy();
        ThinWebViewConstraints constraints = new ThinWebViewConstraints();
        constraints.supportsOpacity = true;
        constraints.backgroundColor = mBackgroundColor;
        mThinWebView =
                ThinWebViewFactory.create(
                        mContext,
                        constraints,
                        assumeNonNull(mWindowAndroid.getIntentRequestTracker()),
                        /* enablePermissionRequests= */ true);
        mWebViewResizingHelper.reset();
    }

    static void setInTestModeForTesting() {
        sInTestMode = true;
        ResettersForTesting.register(() -> sInTestMode = false);
    }
}
