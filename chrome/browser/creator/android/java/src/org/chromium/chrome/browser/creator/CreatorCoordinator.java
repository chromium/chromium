// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
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
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Sets up the Coordinator for Cormorant Creator surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorCoordinator implements FeedAutoplaySettingsDelegate,
                                           FeedContentFirstLoadWatcher,
                                           View.OnLayoutChangeListener {
    private final ViewGroup mCreatorViewGroup;
    private CreatorMediator mMediator;
    private CreatorTabMediator mTabMediator;
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
    private final CreatorSnackbarController mCreatorSnackbarController;
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

    private EmptyBottomSheetObserver mSheetObserver;
    private ContentView mContentView;
    private WebContents mWebContents;
    private int mCurrentMaxViewHeight;
    private CreatorTabSheetContent mSheetContent;
    private boolean mPeeked;
    private boolean mFullyOpened;
    private WebContentsCreator mCreatorWebContents;
    private NewTabCreator mCreatorOpenTab;
    private final UnownedUserDataSupplier<ShareDelegate> mBottomsheetShareDelegateSupplier;
    private GURL mBottomSheetUrl;

    private static final String CREATOR_PROFILE_ID = "CreatorProfileView";

    /**
     * Constructor for the CreatorCoordinator.
     *
     * @param activity The Creator Activity this is a part of.
     * @param webFeedId The ID that is is used to create the feed.
     * @param snackbarManager the snackbarManager that is used for the feed.
     * @param windowAndroid the window needed by the feed
     * @param profile The Profile of the user.
     * @param title The title used by the creator profile.
     * @param url the url used by the creator profile.
     * @param creatorWebContents the interface to generate webcontents for the bottomsheet.
     * @param creatorOpenTab the interface to open urls in a new tab, used by the bottomsheet.
     * @param bottomsheetShareDelegateSupplier an empty share delegate supplier, used by the
     *         bottomsheet.
     */
    public CreatorCoordinator(Activity activity, byte[] webFeedId, SnackbarManager snackbarManager,
            WindowAndroid windowAndroid, Profile profile, String title, String url,
            WebContentsCreator creatorWebContents, NewTabCreator creatorOpenTab,
            UnownedUserDataSupplier<ShareDelegate> bottomsheetShareDelegateSupplier) {
        mActivity = activity;
        mWebFeedId = webFeedId;
        mProfile = profile;
        mSnackbarManager = snackbarManager;
        mWindowAndroid = windowAndroid;
        mTitle = title;
        mUrl = url;
        mRecyclerView = setUpView();
        mCreatorWebContents = creatorWebContents;
        mCreatorOpenTab = creatorOpenTab;
        mBottomsheetShareDelegateSupplier = bottomsheetShareDelegateSupplier;
        mCreatorSnackbarController = new CreatorSnackbarController(mActivity, mSnackbarManager);

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

        mMediator = new CreatorMediator(mActivity, mCreatorModel, mCreatorSnackbarController);
    }

    /**
     * Create a FeedStream and bind it to the RecyclerView
     *
     * @param FeedActionDelegate Interface for Feed actions implemented by the Browser.
     * @param HelpAndFeedbackLauncher Interface for launching a help and feedback page.
     * @param Supplier<ShareDelegate> Supplier of the interface to expose sharing.
     */
    public void initFeedStream(FeedActionDelegate feedActionDelegate,
            HelpAndFeedbackLauncher helpAndFeedbackLauncher,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        mStream = new FeedStream(mActivity, mSnackbarManager, mBottomSheetController,
                /* isPlaceholderShownInitially */ false, mWindowAndroid,
                /* shareSupplier */ shareDelegateSupplier, StreamKind.SINGLE_WEB_FEED,
                /* FeedAutoplaySettingsDelegate */ this, feedActionDelegate,
                helpAndFeedbackLauncher,
                /* FeedContentFirstLoadWatcher */ this,
                /* streamsMediator */ new StreamsMediatorImpl(), mWebFeedId);

        mStream.bind(mRecyclerView, mContentManager, /*FeedScrollState*/ null, mSurfaceScope,
                mHybridListRenderer, new FeedLaunchReliabilityLogger() {}, mHeaderCount,
                /* shouldScrollToTop */ false);
    }

    private class StreamsMediatorImpl implements Stream.StreamsMediator {
        @Override
        public void disableFollowButton() {
            mRecyclerView.findViewById(R.id.creator_follow_button).setEnabled(false);
            mRecyclerView.findViewById(R.id.creator_following_button).setEnabled(false);
        }
    }

    public ViewGroup getView() {
        return mCreatorViewGroup;
    }

    @VisibleForTesting
    public View getProfileView() {
        return mProfileView;
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

    /**
     * Set up the bottom sheet for this activity.
     */
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
                        recyclerView.canScrollVertically(-1));
            }
        });
    }

    /**
     * Launches autoplay settings activity.
     */
    @Override
    public void launchAutoplaySettings() {}
    @Override
    public void nonNativeContentLoaded(@StreamKind int kind) {}

    /**
     * Entry point for preview tab flow. This will create an creator tab and show it in the
     * bottom sheet.
     *
     * @param url The URL to be shown.
     */
    public void requestOpenSheet(GURL url) {
        mBottomSheetUrl = url;
        if (mTabMediator == null) {
            float topControlsHeight =
                    mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                    / mWindowAndroid.getDisplay().getDipScale();
            mTabMediator = new CreatorTabMediator(
                    mBottomSheetController, new FaviconLoader(mActivity), (int) topControlsHeight);
        }
        if (mWebContents == null) {
            assert mSheetContent == null;
            createWebContents();
            mSheetObserver = new EmptyBottomSheetObserver() {
                @Override
                public void onSheetContentChanged(BottomSheetContent newContent) {
                    if (newContent != mSheetContent) {
                        mPeeked = false;
                        destroyWebContents();
                    }
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    if (mSheetContent == null) return;
                    switch (newState) {
                        case BottomSheetController.SheetState.PEEK:
                            if (!mPeeked) {
                                mPeeked = true;
                            }
                            break;
                        case BottomSheetController.SheetState.FULL:
                            if (!mFullyOpened) {
                                mFullyOpened = true;
                            }
                            break;
                    }
                }

                @Override
                public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                    if (mSheetContent == null) return;
                    mSheetContent.showOpenInNewTabButton(heightFraction);
                }
            };
            mBottomSheetController.addObserver(mSheetObserver);
            IntentRequestTracker intentRequestTracker = mWindowAndroid.getIntentRequestTracker();
            assert intentRequestTracker
                    != null : "ActivityWindowAndroid must have a IntentRequestTracker.";
            mSheetContent = new CreatorTabSheetContent(mActivity, this::openInNewTab,
                    this::onToolbarClick, this::close, getMaxViewHeight(), intentRequestTracker,
                    mBottomsheetShareDelegateSupplier);
            mTabMediator.init(mWebContents, mContentView, mSheetContent, mProfile);
            mLayoutView.addOnLayoutChangeListener(this);
        }

        mPeeked = false;
        mFullyOpened = false;
        mTabMediator.requestShowContent(url, mTitle);
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mSheetContent == null) return;

        // It may not be possible to update the content height when the actual height changes
        // due to the current tab not being ready yet. Try it later again when the tab
        // (hence MaxViewHeight) becomes valid.
        int maxViewHeight = getMaxViewHeight();
        if (maxViewHeight == 0 || mCurrentMaxViewHeight == maxViewHeight) return;
        mSheetContent.updateContentHeight(maxViewHeight);
        mCurrentMaxViewHeight = maxViewHeight;
    }

    /**
     * @return The maximum base view height for sheet content view.
     * */
    private int getMaxViewHeight() {
        return mCreatorViewGroup.getHeight();
    }

    /**
     * Close the bottomsheet tab.
     */
    public void close() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }

    private void openInNewTab() {
        String url = mBottomSheetUrl.isValid() ? mBottomSheetUrl.getSpec() : mUrl;
        mBottomSheetController.hideContent(
                mSheetContent, /* animate= */ true, StateChangeReason.PROMOTE_TAB);
        mCreatorOpenTab.createNewTab(new LoadUrlParams(url));
    }

    private void onToolbarClick() {
        int state = mBottomSheetController.getSheetState();
        if (state == BottomSheetController.SheetState.PEEK) {
            mBottomSheetController.expandSheet();
        } else if (state == BottomSheetController.SheetState.FULL) {
            mBottomSheetController.collapseSheet(true);
        }
    }
    private void createWebContents() {
        assert mWebContents == null;

        // Creates an initially hidden WebContents which gets shown when the panel is opened.
        mWebContents = mCreatorWebContents.createWebContents();

        mContentView = ContentView.createContentView(
                mActivity, null /* eventOffsetHandler */, mWebContents);

        mWebContents.initialize(VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(mContentView), mContentView, mWindowAndroid,
                WebContents.createDefaultInternalsHolder());
    }

    private void destroyWebContents() {
        mSheetContent = null; // Will be destroyed by BottomSheet controller.

        mPeeked = false;
        mFullyOpened = false;

        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
            mContentView = null;
        }

        if (mMediator != null) mTabMediator.destroyContent();

        mLayoutView.removeOnLayoutChangeListener(this);
        if (mSheetObserver != null) mBottomSheetController.removeObserver(mSheetObserver);
    }
    /**
     * Helper class to generate a favicon for a given URL and resize it to the desired dimensions
     * for displaying it on the image view.
     */
    static class FaviconLoader {
        private final Context mContext;
        private final FaviconHelper mFaviconHelper;
        private final RoundedIconGenerator mIconGenerator;
        private final int mFaviconSize;

        /**
         * The FaviconLoader constructor.
         * @param context The context where the Favicon will be loaded.
         */
        public FaviconLoader(Context context) {
            mContext = context;
            mFaviconHelper = new FaviconHelper();
            mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
            mFaviconSize =
                    mContext.getResources().getDimensionPixelSize(R.dimen.preview_tab_favicon_size);
        }

        /**
         * Generates a favicon for a given URL. If no favicon was could be found or generated from
         * the URL, a default favicon will be shown.
         *
         * @param url The URL for which favicon is to be generated.
         * @param callback The callback to be invoked to display the final image.
         * @param profile The profile for which favicon service is used.
         */
        public void loadFavicon(final GURL url, Callback<Drawable> callback, Profile profile) {
            assert profile != null;
            FaviconHelper.FaviconImageCallback imageCallback = (bitmap, iconUrl) -> {
                Drawable drawable;
                if (bitmap != null) {
                    drawable = FaviconUtils.createRoundedBitmapDrawable(
                            mContext.getResources(), bitmap);
                } else {
                    drawable = UiUtils.getTintedDrawable(mContext, R.drawable.ic_globe_24dp,
                            R.color.default_icon_color_tint_list);
                }

                callback.onResult(drawable);
            };

            mFaviconHelper.getLocalFaviconImageForURL(profile, url, mFaviconSize, imageCallback);
        }
    }
}
