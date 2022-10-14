// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.FeedSurfaceScopeDependencyProvider;
import org.chromium.chrome.browser.feed.FeedSurfaceTracker;
import org.chromium.chrome.browser.feed.NativeViewListRenderer;
import org.chromium.chrome.browser.feed.NtpListContentManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Sets up the Coordinator for Cormorant Creator surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorCoordinator {
    private static final String NATIVE_CONTENT_ID = "0";

    private CreatorMediator mMediator;
    private Activity mActivity;
    private NtpListContentManager mContentManager;
    private RecyclerView mRecyclerView;
    private HybridListRenderer mHybridListRenderer;
    private SurfaceScope mSurfaceScope;
    private FeedSurfaceScopeDependencyProvider mDependencyProvider;

    private final ViewGroup mViewGroup;

    public CreatorCoordinator(Activity activity) {
        mActivity = activity;
        mRecyclerView = setUpView();
        List<NtpListContentManager.FeedContent> contentPreviewsList = new ArrayList<>();

        // Add empty state to Content Manager
        contentPreviewsList.add(new NtpListContentManager.NativeViewContent(
                getLateralPaddingsPx(), NATIVE_CONTENT_ID, R.layout.no_content_v2));
        mContentManager.addContents(0, contentPreviewsList);

        // Inflate the XML.
        mViewGroup =
                (ViewGroup) LayoutInflater.from(mActivity).inflate(R.layout.creator_activity, null);
        mViewGroup.addView(mRecyclerView);

        mMediator = new CreatorMediator(mActivity);
    }

    public ViewGroup getView() {
        return mViewGroup;
    }

    private RecyclerView setUpView() {
        // TODO(crbug.com/1374744): Refactor NTP naming out of the general Feed code.
        mContentManager = new NtpListContentManager();
        ProcessScope processScope = FeedSurfaceTracker.getInstance().getXSurfaceProcessScope();

        if (processScope != null) {
            mDependencyProvider =
                    new FeedSurfaceScopeDependencyProvider(mActivity, mActivity, false);
            mSurfaceScope = processScope.obtainSurfaceScope(mDependencyProvider);
        } else {
            mDependencyProvider = null;
            mSurfaceScope = null;
        }

        if (mSurfaceScope != null) {
            mHybridListRenderer = mSurfaceScope.provideListRenderer();
        } else {
            mHybridListRenderer = new NativeViewListRenderer(mActivity);
        }

        RecyclerView view;
        if (mHybridListRenderer != null) {
            view = (RecyclerView) mHybridListRenderer.bind(
                    mContentManager, /* mViewportView */ null, /* useStaggeredLayout */ false);
            view.setId(R.id.creator_feed_stream_recycler_view);
            view.setClipToPadding(false);
            view.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(mActivity));
        } else {
            view = null;
        }
        return view;
    }

    private int getLateralPaddingsPx() {
        return mActivity.getResources().getDimensionPixelSize(R.dimen.content_previews_padding);
    }
}
