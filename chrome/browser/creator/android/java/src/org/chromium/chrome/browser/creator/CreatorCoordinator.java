// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedAutoplaySettingsDelegate;
import org.chromium.chrome.browser.feed.FeedContentFirstLoadWatcher;
import org.chromium.chrome.browser.feed.FeedStream;
import org.chromium.chrome.browser.feed.FeedSurfaceScopeDependencyProvider;
import org.chromium.chrome.browser.feed.FeedSurfaceTracker;
import org.chromium.chrome.browser.feed.NativeViewListRenderer;
import org.chromium.chrome.browser.feed.NtpListContentManager;
import org.chromium.chrome.browser.feed.Stream;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionStatus;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.ColorUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Sets up the Coordinator for Cormorant Creator surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorCoordinator
        implements FeedAutoplaySettingsDelegate, FeedContentFirstLoadWatcher {
    private final ViewGroup mCreatorViewGroup;
    private CreatorMediator mMediator;
    private Activity mActivity;
    private NtpListContentManager mContentManager;
    private RecyclerView mRecyclerView;
    private View mProfileView;
    private ViewGroup mLayoutView;
    private HybridListRenderer mHybridListRenderer;
    private SurfaceScope mSurfaceScope;
    private FeedSurfaceScopeDependencyProvider mDependencyProvider;
    private byte[] mWebFeedId;
    private PropertyModel mCreatorModel;
    private boolean mIsFollowed;
    private PropertyModelChangeProcessor<PropertyModel, CreatorProfileView, PropertyKey>
            mCreatorProfileModelChangeProcessor;
    private PropertyModelChangeProcessor<PropertyModel, CreatorToolbarView, PropertyKey>
            mCreatorToolbarModelChangeProcessor;

    private final SnackbarManager mSnackbarManager;
    private final WindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private ScrimCoordinator mScrim;
    private ViewGroup mBottomSheetContainer;
    private ViewGroup mLayout;
    private Profile mProfile;
    private Stream mStream;
    private String mTitle;
    private String mUrl;
    private int mHeaderCount;

    private static final String CREATOR_PROFILE_ID = "CreatorProfileView";

    public CreatorCoordinator(Activity activity, byte[] webFeedId, SnackbarManager snackbarManager,
            WindowAndroid windowAndroid, Profile profile, String title, String url) {
        mActivity = activity;
        mWebFeedId = webFeedId;
        mProfile = profile;
        mSnackbarManager = snackbarManager;
        mWindowAndroid = windowAndroid;
        mTitle = title;
        mUrl = url;
        mRecyclerView = setUpView();

        mProfileView =
                (View) LayoutInflater.from(mActivity).inflate(R.layout.creator_profile, null);
        List<NtpListContentManager.FeedContent> contentPreviewsList = new ArrayList<>();
        contentPreviewsList.add(new NtpListContentManager.NativeViewContent(
                getContentPreviewsPaddingPx(), CREATOR_PROFILE_ID, mProfileView));
        mContentManager.addContents(0, contentPreviewsList);
        mHeaderCount = 1;

        // Inflate the XML
        mCreatorViewGroup =
                (ViewGroup) LayoutInflater.from(mActivity).inflate(R.layout.creator_activity, null);
        mLayoutView = mCreatorViewGroup.findViewById(R.id.creator_layout);
        mLayoutView.addView(mRecyclerView);

        // TODO(crbug.com/1377069): Add a JNI to get the follow status from CreatorBridge instead
        getIsFollowedStatus();
        initBottomSheet();

        // Generate Creator Model
        mCreatorModel = generateCreatorModel(mWebFeedId, mTitle, mUrl, mIsFollowed);
        mCreatorProfileModelChangeProcessor = PropertyModelChangeProcessor.create(
                mCreatorModel, (CreatorProfileView) mProfileView, CreatorProfileViewBinder::bind);
        mCreatorToolbarModelChangeProcessor = PropertyModelChangeProcessor.create(
                mCreatorModel, (CreatorToolbarView) mLayoutView, CreatorToolbarViewBinder::bind);
        setUpToolbarListener();

        mMediator = new CreatorMediator(mActivity, mCreatorModel);
    }

    // Create a FeedStream and bind it to the RecyclerView
    public void initFeedStream(FeedActionDelegate feedActionDelegate,
            HelpAndFeedbackLauncher helpAndFeedbackLauncher,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        mStream = new FeedStream(mActivity, mSnackbarManager, mBottomSheetController,
                /* isPlaceholderShownInitially */ false, mWindowAndroid,
                /* shareSupplier */ shareDelegateSupplier, StreamKind.SINGLE_WEB_FEED,
                /* FeedAutoplaySettingsDelegate */ this, feedActionDelegate,
                helpAndFeedbackLauncher,
                /* FeedContentFirstLoadWatcher */ this,
                /* streamsMediator */ null, mWebFeedId);

        mStream.bind(mRecyclerView, mContentManager, /*FeedScrollState*/ null, mSurfaceScope,
                mHybridListRenderer, new FeedLaunchReliabilityLogger() {}, mHeaderCount,
                /* shouldScrollToTop */ false);
    }

    public ViewGroup getView() {
        return mCreatorViewGroup;
    }

    public PropertyModel getCreatorModel() {
        return mCreatorModel;
    }

    public BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    private RecyclerView setUpView() {
        // TODO(crbug.com/1374744): Refactor NTP naming out of the general Feed code.
        mContentManager = new NtpListContentManager();
        ProcessScope processScope = FeedSurfaceTracker.getInstance().getXSurfaceProcessScope();

        if (processScope != null) {
            mDependencyProvider = new FeedSurfaceScopeDependencyProvider(
                    mActivity, mActivity, ColorUtils.inNightMode(mActivity));
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

    private int getContentPreviewsPaddingPx() {
        // Return 16dp
        return mActivity.getResources().getDimensionPixelSize(R.dimen.content_previews_padding);
    }

    private PropertyModel generateCreatorModel(
            byte[] webFeedId, String title, String url, boolean isFollowed) {
        PropertyModel model = new PropertyModel.Builder(CreatorProperties.ALL_KEYS)
                                      .with(CreatorProperties.WEB_FEED_ID_KEY, webFeedId)
                                      .with(CreatorProperties.TITLE_KEY, title)
                                      .with(CreatorProperties.URL_KEY, url)
                                      .with(CreatorProperties.IS_FOLLOWED_KEY, isFollowed)
                                      .with(CreatorProperties.IS_TOOLBAR_VISIBLE_KEY, false)
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

    /** Set up the bottom sheet for this activity. */
    private void initBottomSheet() {
        mScrim = new ScrimCoordinator(mActivity, new ScrimCoordinator.SystemUiScrimDelegate() {
            @Override
            public void setStatusBarScrimFraction(float scrimFraction) {}

            @Override
            public void setNavigationBarScrimFraction(float scrimFraction) {}
        }, mCreatorViewGroup, mActivity.getResources().getColor(R.color.default_scrim_color));

        mBottomSheetContainer = new FrameLayout(mActivity);
        mBottomSheetContainer.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mCreatorViewGroup.addView(mBottomSheetContainer);
        mBottomSheetController = BottomSheetControllerFactory.createBottomSheetController(
                () -> mScrim, (sheet) -> {}, mActivity.getWindow(),
                KeyboardVisibilityDelegate.getInstance(), () -> mBottomSheetContainer);
    }

    private void setUpToolbarListener() {
        mRecyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                mCreatorModel.set(CreatorProperties.IS_TOOLBAR_VISIBLE_KEY,
                        mHybridListRenderer.getListLayoutHelper().findFirstVisibleItemPosition()
                                > 0);
            }
        });
    }

    /** Launches autoplay settings activity. */
    @Override
    public void launchAutoplaySettings() {}
    @Override
    public void nonNativeContentLoaded(@StreamKind int kind) {}
}
