// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feed.FeedSurfaceScopeDependencyProvider;
import org.chromium.chrome.browser.feed.FeedSurfaceTracker;
import org.chromium.chrome.browser.feed.NativeViewListRenderer;
import org.chromium.chrome.browser.feed.NtpListContentManager;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionStatus;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * Sets up the Coordinator for Cormorant Creator surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorCoordinator {
    private static final String NATIVE_CONTENT_ID = "0";

    private final ViewGroup mViewGroup;
    private CreatorMediator mMediator;
    private Activity mActivity;
    private NtpListContentManager mContentManager;
    private RecyclerView mRecyclerView;
    private View mProfileView;
    private HybridListRenderer mHybridListRenderer;
    private SurfaceScope mSurfaceScope;
    private FeedSurfaceScopeDependencyProvider mDependencyProvider;
    private byte[] mWebFeedId;
    private PropertyModel mCreatorProfileModel;
    private PropertyModelChangeProcessor<PropertyModel, CreatorProfileView, PropertyKey>
            mCreatorProfileModelChangeProcessor;
    private boolean mIsFollowed;

    public CreatorCoordinator(Activity activity, byte[] webFeedId) {
        mActivity = activity;
        mWebFeedId = webFeedId;

        mRecyclerView = setUpView();
        List<NtpListContentManager.FeedContent> contentPreviewsList = new ArrayList<>();
        // Add empty state to Content Manager
        contentPreviewsList.add(new NtpListContentManager.NativeViewContent(
                getContentPreviewsPaddingPx(), NATIVE_CONTENT_ID, R.layout.no_content_v2));
        mContentManager.addContents(0, contentPreviewsList);

        // Inflate the XML
        mViewGroup =
                (ViewGroup) LayoutInflater.from(mActivity).inflate(R.layout.creator_activity, null);
        mViewGroup.addView(mRecyclerView);
        mProfileView = mViewGroup.findViewById(R.id.creator_profile);

        // TODO(crbug.com/1377069): Add a JNI to get the follow status from CreatorBridge instead
        getIsFollowedStatus();

        // Generate CreatorProfileModel
        mCreatorProfileModel =
                generateCreatorProfileModel(mWebFeedId, /* title */ "", /* url */ "", mIsFollowed);
        mCreatorProfileModelChangeProcessor =
                PropertyModelChangeProcessor.create(mCreatorProfileModel,
                        (CreatorProfileView) mProfileView, CreatorProfileViewBinder::bind);

        mMediator = new CreatorMediator(mActivity, mCreatorProfileModel);
    }

    public ViewGroup getView() {
        return mViewGroup;
    }

    public PropertyModel getCreatorProfileModel() {
        return mCreatorProfileModel;
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
            view.setPadding(view.getPaddingLeft(), getContentPreviewsPaddingPx(),
                    view.getPaddingRight(), view.getPaddingBottom());
        } else {
            view = null;
        }

        return view;
    }

    private int getContentPreviewsPaddingPx() {
        // Return 16dp
        return mActivity.getResources().getDimensionPixelSize(R.dimen.content_previews_padding);
    }

    private PropertyModel generateCreatorProfileModel(
            byte[] webFeedId, String title, String url, boolean isFollowed) {
        PropertyModel model = new PropertyModel.Builder(CreatorProfileProperties.ALL_KEYS)
                                      .with(CreatorProfileProperties.WEB_FEED_ID_KEY, webFeedId)
                                      .with(CreatorProfileProperties.TITLE_KEY, title)
                                      .with(CreatorProfileProperties.URL_KEY, url)
                                      .with(CreatorProfileProperties.IS_FOLLOWED_KEY, isFollowed)
                                      .build();
        return model;
    }

    private void getIsFollowedStatus() {
        Callback<WebFeedMetadata> metadata_callback = result -> {
            @WebFeedSubscriptionStatus
            int subscriptionStatus =
                    result == null ? WebFeedSubscriptionStatus.UNKNOWN : result.subscriptionStatus;
            if (subscriptionStatus == WebFeedSubscriptionStatus.UNKNOWN
                    || subscriptionStatus == WebFeedSubscriptionStatus.NOT_SUBSCRIBED) {
                mIsFollowed = false;
            } else if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBED) {
                mIsFollowed = true;
            }
        };

        WebFeedBridge.getWebFeedMetadata(mWebFeedId, metadata_callback);
    }
}
