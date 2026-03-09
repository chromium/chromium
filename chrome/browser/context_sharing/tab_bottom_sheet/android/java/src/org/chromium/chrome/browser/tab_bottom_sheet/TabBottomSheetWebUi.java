// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/** Abstract class for Tab Bottom Sheet toolbars. */
@NullMarked
public class TabBottomSheetWebUi {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;

    TabBottomSheetWebUi(Context context, WindowAndroid windowAndroid) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        resetThinWebView();
    }

    @SuppressLint("ClickableViewAccessibility")
    void setWebContents(@Nullable WebContents webContents) {
        mWebContents = webContents;
        if (mWebContents != null) {
            ContentView contentView = ContentView.createContentView(mContext, null);

            // Prevent bottom sheet from intercepting touch events.
            View.OnTouchListener touchListener =
                    (v, event) -> {
                        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                            v.getParent().requestDisallowInterceptTouchEvent(true);
                        }
                        return false;
                    };
            contentView.setOnTouchListener(touchListener);

            mWebContents.setDelegates(
                    VersionInfo.getProductVersion(),
                    ViewAndroidDelegate.createBasicDelegate(contentView),
                    contentView,
                    mWindowAndroid,
                    WebContents.createDefaultInternalsHolder());
            contentView.setWebContents(mWebContents);
            mThinWebView.attachWebContents(
                    mWebContents,
                    contentView,
                    /* delegate= */ null,
                    /* contextMenuPopulatorFactory= */ null,
                    /* selectionDropdownMenuDelegate= */ null);
        } else {
            resetThinWebView();
        }
    }

    @Nullable WebContents getWebContents() {
        return mWebContents;
    }

    /** Sets the size of the web contents. */
    void setInsets(int top, int left, int bottom, int right) {
        mThinWebView.setInsets(top, left, bottom, right);
    }

    void destroy() {
        // We expect the life cycle of webContents to be managed by native.
        mWebContents = null;
        mThinWebView.destroy();
    }

    View getWebUiView() {
        return mThinWebView.getView();
    }

    private void resetThinWebView() {
        if (mThinWebView != null) mThinWebView.destroy();
        ThinWebViewConstraints constraints = new ThinWebViewConstraints();
        constraints.supportsOpacity = true;
        mThinWebView =
                ThinWebViewFactory.create(
                        mContext,
                        constraints,
                        assumeNonNull(mWindowAndroid.getIntentRequestTracker()));
    }
}
