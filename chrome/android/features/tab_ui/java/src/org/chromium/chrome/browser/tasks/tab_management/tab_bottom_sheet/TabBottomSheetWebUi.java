// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
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
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private @Nullable WebContents mWebContents;
    private @Nullable ThinWebView mThinWebView;

    TabBottomSheetWebUi(Context context, Profile profile, WindowAndroid windowAndroid) {
        mContext = context;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
    }

    void initialize() {
        mWebContents = WebContentsFactory.createWebContents(mProfile, true, true);
        ContentView contentView = ContentView.createContentView(mContext, mWebContents);
        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                mWindowAndroid,
                WebContents.createDefaultInternalsHolder());
        mThinWebView =
                ThinWebViewFactory.create(
                        mContext,
                        new ThinWebViewConstraints(),
                        assumeNonNull(mWindowAndroid.getIntentRequestTracker()));
        mThinWebView.attachWebContents(mWebContents, contentView, null);
    }

    void destroy() {
        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
        }
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
    }

    @Nullable View getWebUiView() {
        if (mThinWebView == null) {
            return null;
        }
        return mThinWebView.getView();
    }
}
