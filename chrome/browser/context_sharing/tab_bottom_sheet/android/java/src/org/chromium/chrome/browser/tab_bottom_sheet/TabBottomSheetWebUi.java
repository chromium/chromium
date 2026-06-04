// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isActivityFinishingOrDestroyed;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.components.thinwebview.internal.ThinWebViewContextMenuItemDelegate;
import org.chromium.content_public.browser.ImeAdapter;
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

    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private @Nullable ContentView mContentView;

    TabBottomSheetWebUi(
            Context context,
            View containerView,
            WindowAndroid windowAndroid,
            ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @ColorInt int backgroundColor) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mContextMenuPopulatorFactory = contextMenuPopulatorFactory;
        mBackgroundColor = backgroundColor;
        mWebViewResizingHelper =
                new WebViewResizingHelper(containerView, windowAndroid, backgroundColor);
    }

    @SuppressLint("ClickableViewAccessibility")
    void setWebContents(@Nullable WebContents webContents, boolean requestFocus) {
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
            ensureThinWebViewCreated();
            if (mThinWebView != null) {
                mThinWebView.attachWebContents(
                        mWebContents,
                        contentView,
                        new ThinWebViewAttachParams.Builder()
                                .setContextMenuPopulatorFactory(mContextMenuPopulatorFactory)
                                .setSupportTheming(true)
                                .build());
                mWebViewResizingHelper.setThinWebView(mThinWebView, mWebContents);
                setAllowFullscreenIme(
                        mContext.getResources().getConfiguration().orientation
                                == Configuration.ORIENTATION_LANDSCAPE);
            }

            if (requestFocus) {
                // Only request focus once the web contents have been attached to the activity's
                // layout
                // tree.
                View currentlyFocusedView =
                        assertNonNull(mWindowAndroid.getActivity().get()).getCurrentFocus();
                if (currentlyFocusedView != null) {
                    currentlyFocusedView.clearFocus();
                }
                contentView.requestFocus();
            }
        } else {
            destroyThinWebView();
        }
    }

    void setAllowFullscreenIme(boolean allow) {
        if (mWebContents == null) return;
        ImeAdapter adapter = ImeAdapter.fromWebContents(mWebContents);
        if (adapter != null) {
            adapter.setAllowFullscreenIme(allow);
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
        destroyThinWebView();
    }

    View getWebUiView() {
        return mWebViewResizingHelper.getResizingContainer();
    }

    @VisibleForTesting
    ContentView createContentView(Context context, WebContents webContents) {
        return ContentView.createContentView(context, webContents);
    }

    private void ensureThinWebViewCreated() {
        if (mThinWebView != null) {
            return;
        }

        if (isActivityFinishingOrDestroyed(mWindowAndroid)) {
            return;
        }

        ThinWebViewConstraints constraints = new ThinWebViewConstraints();
        constraints.supportsOpacity = true;
        constraints.backgroundColor = mBackgroundColor;
        constraints.ignoreSizeChanges = true;
        mThinWebView =
                ThinWebViewFactory.create(
                        mContext,
                        constraints,
                        assumeNonNull(mWindowAndroid.getIntentRequestTracker()),
                        /* enablePermissionRequests= */ true);
        mWebViewResizingHelper.setThinWebView(mThinWebView, mWebContents);
    }

    private void destroyThinWebView() {
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
        mWebViewResizingHelper.reset();
    }

    @Nullable ThinWebView getThinWebViewForTesting() {
        return mThinWebView;
    }

    static void setInTestModeForTesting() {
        sInTestMode = true;
        ResettersForTesting.register(() -> sInTestMode = false);
    }
}
