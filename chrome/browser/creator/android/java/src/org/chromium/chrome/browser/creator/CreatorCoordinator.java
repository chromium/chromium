// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedContentFirstLoadWatcher;
import org.chromium.chrome.browser.feed.FeedListContentManager;
import org.chromium.chrome.browser.feed.FeedListContentManager.FeedContent;
import org.chromium.chrome.browser.feed.FeedStream;
import org.chromium.chrome.browser.feed.FeedStreamViewResizer;
import org.chromium.chrome.browser.feed.FeedSurfaceRendererBridge;
import org.chromium.chrome.browser.feed.FeedSurfaceScopeDependencyProviderImpl;
import org.chromium.chrome.browser.feed.FeedSurfaceTracker;
import org.chromium.chrome.browser.feed.NativeViewListRenderer;
import org.chromium.chrome.browser.feed.SingleWebFeedEntryPoint;
import org.chromium.chrome.browser.feed.SingleWebFeedParameters;
import org.chromium.chrome.browser.feed.Stream;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.QueryResult;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionStatus;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.url_formatter.UrlFormatter;
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
public class CreatorCoordinator
        implements FeedContentFirstLoadWatcher, View.OnLayoutChangeListener {
    private final ViewGroup mCreatorViewGroup;
    private CreatorMediator mMediator;
    private CreatorTabMediator mTabMediator;
    private Activity mActivity;
    private FeedListContentManager mContentManager;
    private UiConfig mUiConfig;
    private RecyclerView mRecyclerView;
    private View mProfileView;
    private ViewGroup mLayoutView;
    private HybridListRenderer mHybridListRenderer;
    private FeedSurfaceScope mSurfaceScope;
    private FeedSurfaceScopeDependencyProviderImpl mDependencyProvider;
    private PropertyModel mCreatorModel;
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
    private int mEntryPoint;

    private @Nullable FeedStreamViewResizer mStreamViewResizer;

    private static final String CREATOR_PROFILE_ID = "CreatorProfileView";
    private static final String CREATOR_PRIVACY_ID = "CreatorPrivacyId";

    /**
     * Constructor for the CreatorCoordinator.
     *
     * @param activity the Creator Activity this is a part of.
     * @param webFeedId the ID that is is used to create the feed.
     * @param snackbarManager the snackbarManager that is used for the feed.
     * @param windowAndroid the window needed by the feed
     * @param profile the Profile of the user.
     * @param url the url used by the creator profile.
     * @param creatorWebContents the interface to generate webcontents for the bottomsheet.
     * @param creatorOpenTab the interface to open urls in a new tab, used by the bottomsheet.
     * @param bottomsheetShareDelegateSupplier an empty share delegate supplier, used by the
     *         bottomsheet.
     * @param entryPoint the SingleWebFeedEntryPoint has the Activity been launched with.
     * @param isFollowing the initial state of if the creator is being followed.
     */
    public CreatorCoordinator(
            Activity activity,
            byte[] webFeedId,
            SnackbarManager snackbarManager,
            WindowAndroid windowAndroid,
            Profile profile,
            String url,
            WebContentsCreator creatorWebContents,
            NewTabCreator creatorOpenTab,
            UnownedUserDataSupplier<ShareDelegate> bottomsheetShareDelegateSupplier,
            int entryPoint,
            boolean isFollowing,
            SignInInterstitialInitiator signInInterstitialInitiator) {
        mActivity = activity;
        mProfile = profile;
        mSnackbarManager = snackbarManager;
        mWindowAndroid = windowAndroid;
        mRecyclerView = setUpView();
        mCreatorWebContents = creatorWebContents;
        mCreatorOpenTab = creatorOpenTab;
        mBottomsheetShareDelegateSupplier = bottomsheetShareDelegateSupplier;
        mEntryPoint = entryPoint;
        mCreatorSnackbarController = new CreatorSnackbarController(mActivity, mSnackbarManager);

        mProfileView =
                (View) LayoutInflater.from(mActivity).inflate(R.layout.creator_profile, null);
        List<FeedListContentManager.FeedContent> contentPreviewsList = new ArrayList<>();
        contentPreviewsList.add(
                new FeedListContentManager.NativeViewContent(
                        getContentPreviewsPaddingPx(), CREATOR_PROFILE_ID, mProfileView));
        mContentManager.addContents(0, contentPreviewsList);
        mHeaderCount = 1;

        // Inflate the XML
        mCreatorViewGroup =
                (ViewGroup) LayoutInflater.from(mActivity).inflate(R.layout.creator_activity, null);
        mLayoutView = mCreatorViewGroup.findViewById(R.id.creator_layout);
        mUiConfig = new UiConfig(mLayoutView);
        mStreamViewResizer =
                FeedStreamViewResizer.createAndAttach(mActivity, mRecyclerView, mUiConfig);
        mLayoutView.addView(mRecyclerView);

        // Generate Creator Model
        mCreatorModel = generateCreatorModel(webFeedId, url, isFollowing);
        // Attempt to avoid possible extra query if we already have metadata.
        if (webFeedId != null) {
            getWebFeedMetadata();
        }
        initBottomSheet();

        mCreatorProfileModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mCreatorModel,
                        (CreatorProfileView) mProfileView,
                        CreatorProfileViewBinder::bind);
        mCreatorToolbarModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mCreatorModel,
                        (CreatorToolbarView) mLayoutView,
                        CreatorToolbarViewBinder::bind);
        setUpToolbarListener();

        mMediator =
                new CreatorMediator(
                        mActivity,
                        mCreatorModel,
                        mCreatorSnackbarController,
                        signInInterstitialInitiator);
    }

    /**
     * Query for webfeedId if we don't have it, and then create the FeedStream.
     *
     * @param feedActionDelegate Interface for Feed actions implemented by the Browser.
     */
    public void queryFeedStream(
            FeedActionDelegate feedActionDelegate, Supplier<ShareDelegate> shareDelegateSupplier) {
        if (mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY) == null) {
            Callback<QueryResult> queryWebFeedIdCallback =
                    result -> {
                        mCreatorModel.set(
                                CreatorProperties.WEB_FEED_ID_KEY, result.webFeedId.getBytes());
                        mCreatorModel.set(CreatorProperties.TITLE_KEY, result.title);
                        if (TextUtils.isEmpty(mCreatorModel.get(CreatorProperties.URL_KEY))) {
                            mCreatorModel.set(CreatorProperties.URL_KEY, result.url);
                            mCreatorModel.set(
                                    CreatorProperties.FORMATTED_URL_KEY,
                                    UrlFormatter
                                            .formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
                                                    new GURL(result.url)));
                        }
                        initFeedStream(feedActionDelegate, shareDelegateSupplier);
                    };
            WebFeedBridge.queryWebFeed(
                    mCreatorModel.get(CreatorProperties.URL_KEY), queryWebFeedIdCallback);
        } else if (TextUtils.isEmpty(mCreatorModel.get(CreatorProperties.TITLE_KEY))
                || TextUtils.isEmpty(mCreatorModel.get(CreatorProperties.URL_KEY))) {
            Callback<QueryResult> queryWebFeedIdCallback =
                    result -> {
                        mCreatorModel.set(CreatorProperties.TITLE_KEY, result.title);
                        mCreatorModel.set(CreatorProperties.URL_KEY, result.url);
                        mCreatorModel.set(
                                CreatorProperties.FORMATTED_URL_KEY,
                                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
                                        new GURL(result.url)));
                    };
            WebFeedBridge.queryWebFeedId(
                    new String(mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY)),
                    queryWebFeedIdCallback);
            initFeedStream(feedActionDelegate, shareDelegateSupplier);
        }
    }

    /**
     * Create the FeedStream and bind it to the RecyclerView.
     *
     * @param feedActionDelegate Interface for Feed actions implemented by the Browser.
     */
    private void initFeedStream(
            FeedActionDelegate feedActionDelegate, Supplier<ShareDelegate> shareDelegateSupplier) {
        mStream =
                new FeedStream(
                        mActivity,
                        mProfile,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        /* shareSupplier= */ shareDelegateSupplier,
                        StreamKind.SINGLE_WEB_FEED,
                        feedActionDelegate,
                        /* feedContentFirstLoadWatcher= */ this,
                        /* streamsMediator= */ new StreamsMediatorImpl(),
                        new SingleWebFeedParameters(
                                mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY), mEntryPoint),
                        new FeedSurfaceRendererBridge.Factory() {});

        if (mEntryPoint == SingleWebFeedEntryPoint.MENU) {
            mStream.addOnContentChangedListener(new ContentChangedListener());
        }

        mStream.bind(
                mRecyclerView,
                mContentManager,
                /* feedScrollState= */ null,
                mSurfaceScope,
                mHybridListRenderer,
                null,
                mHeaderCount);
    }

    private class StreamsMediatorImpl implements Stream.StreamsMediator {
        @Override
        public void disableFollowButton() {
            mProfileView.findViewById(R.id.creator_follow_button).setEnabled(false);
            mProfileView.findViewById(R.id.creator_following_button).setEnabled(false);
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
        // TODO(crbug.com/40872531): Refactor NTP naming out of the general Feed code.
        mContentManager = new FeedListContentManager();
        ProcessScope processScope = FeedSurfaceTracker.getInstance().getXSurfaceProcessScope();

        if (processScope != null) {
            mDependencyProvider =
                    new FeedSurfaceScopeDependencyProviderImpl(
                            mActivity, mActivity, ColorUtils.inNightMode(mActivity));
            mSurfaceScope = processScope.obtainFeedSurfaceScope(mDependencyProvider);
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
            view =
                    (RecyclerView)
                            mHybridListRenderer.bind(
                                    mContentManager,
                                    /* mViewportView= */ null,
                                    /* useStaggeredLayout= */ false);
            view.setId(R.id.creator_feed_stream_recycler_view);
            view.setClipToPadding(false);
            view.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(mActivity));
            view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        } else {
            view = null;
        }

        return view;
    }

    private int getContentPreviewsPaddingPx() {
        return mActivity.getResources().getDimensionPixelSize(R.dimen.content_previews_padding);
    }

    private PropertyModel generateCreatorModel(byte[] webFeedId, String url, boolean following) {
        String formattedUrl =
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(new GURL(url));
        PropertyModel model =
                new PropertyModel.Builder(CreatorProperties.ALL_KEYS)
                        .with(CreatorProperties.WEB_FEED_ID_KEY, webFeedId)
                        .with(CreatorProperties.URL_KEY, url)
                        .with(CreatorProperties.IS_FOLLOWED_KEY, following)
                        .with(CreatorProperties.IS_TOOLBAR_VISIBLE_KEY, false)
                        .with(CreatorProperties.FORMATTED_URL_KEY, formattedUrl)
                        .build();
        return model;
    }

    private void getWebFeedMetadata() {
        Callback<WebFeedMetadata> metadata_callback =
                result -> {
                    @WebFeedSubscriptionStatus
                    int subscriptionStatus =
                            result == null
                                    ? WebFeedSubscriptionStatus.UNKNOWN
                                    : result.subscriptionStatus;
                    if (subscriptionStatus == WebFeedSubscriptionStatus.UNKNOWN
                            || subscriptionStatus == WebFeedSubscriptionStatus.NOT_SUBSCRIBED) {
                        mCreatorModel.set(CreatorProperties.IS_FOLLOWED_KEY, false);
                    } else if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBED) {
                        mCreatorModel.set(CreatorProperties.IS_FOLLOWED_KEY, true);
                    }
                    if (TextUtils.isEmpty(mCreatorModel.get(CreatorProperties.TITLE_KEY))
                            && TextUtils.isEmpty(result.title)) {
                        mCreatorModel.set(CreatorProperties.TITLE_KEY, result.title);
                    }
                    if (TextUtils.isEmpty(mCreatorModel.get(CreatorProperties.URL_KEY))
                            && result.visitUrl.isValid()) {
                        mCreatorModel.set(CreatorProperties.URL_KEY, result.visitUrl.getSpec());
                        mCreatorModel.set(
                                CreatorProperties.FORMATTED_URL_KEY,
                                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
                                        result.visitUrl));
                    }
                };
        WebFeedBridge.getWebFeedMetadata(
                mCreatorModel.get(CreatorProperties.WEB_FEED_ID_KEY), metadata_callback);
    }

    /** Set up the bottom sheet for this activity. */
    private void initBottomSheet() {
        mScrim =
                new ScrimCoordinator(
                        mActivity,
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {}

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {}
                        },
                        mCreatorViewGroup,
                        mActivity.getColor(R.color.default_scrim_color));

        mBottomSheetContainer = new FrameLayout(mActivity);
        mBottomSheetContainer.setId(R.id.creator_content_preview_bottom_sheet);
        mBottomSheetContainer.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mCreatorViewGroup.addView(mBottomSheetContainer);
        mBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrim,
                        (sheet) -> {},
                        mActivity.getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> mBottomSheetContainer,
                        () -> 0,
                        /* desktopWindowStateProvider= */ null);
    }

    private void setUpToolbarListener() {
        mRecyclerView.addOnScrollListener(
                new OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        mCreatorModel.set(
                                CreatorProperties.IS_TOOLBAR_VISIBLE_KEY,
                                recyclerView.canScrollVertically(-1));
                    }
                });
    }

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
            mTabMediator =
                    new CreatorTabMediator(
                            mBottomSheetController,
                            new FaviconLoader(mActivity),
                            (int) topControlsHeight);
        }
        if (mWebContents == null) {
            assert mSheetContent == null;
            createWebContents();
            mSheetObserver =
                    new EmptyBottomSheetObserver() {
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
            assert intentRequestTracker != null
                    : "ActivityWindowAndroid must have a IntentRequestTracker.";
            mSheetContent =
                    new CreatorTabSheetContent(
                            mActivity,
                            this::openInNewTab,
                            this::onToolbarClick,
                            this::close,
                            getMaxViewHeight(),
                            intentRequestTracker,
                            mBottomsheetShareDelegateSupplier);
            mTabMediator.init(mWebContents, mContentView, mSheetContent, mProfile);
            mLayoutView.addOnLayoutChangeListener(this);
        }

        mPeeked = false;
        mFullyOpened = false;
        mTabMediator.requestShowContent(url, mCreatorModel.get(CreatorProperties.TITLE_KEY));
    }

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
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

    /** Close the bottomsheet tab. */
    public void close() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }

    private void openInNewTab() {
        String url =
                mBottomSheetUrl.isValid()
                        ? mBottomSheetUrl.getSpec()
                        : mCreatorModel.get(CreatorProperties.URL_KEY);
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

        mContentView = ContentView.createContentView(mActivity, mWebContents);

        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(mContentView),
                mContentView,
                mWindowAndroid,
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

    void setStreamForTest(Stream stream) {
        mStream = stream;
    }

    class ContentChangedListener implements Stream.ContentChangedListener {
        @Override
        public void onContentChanged(List<FeedContent> feedContents) {
            if (feedContents == null) return;
            boolean hasError = false;
            // Assume native cards beyond the header are errors.
            for (int i = mHeaderCount; i < feedContents.size(); i++) {
                FeedContent content = feedContents.get(i);
                if (content.isNativeView()) {
                    hasError = true;
                    break;
                }
            }
            // If no error cards are found, then remove the listener and add privacy header.
            if (!hasError) {
                List<FeedContent> privacyList = new ArrayList<>();
                View privacyView =
                        LayoutInflater.from(mActivity).inflate(R.layout.creator_privacy, null);
                privacyList.add(
                        new FeedListContentManager.NativeViewContent(
                                getContentPreviewsPaddingPx(), CREATOR_PRIVACY_ID, privacyView));
                mContentManager.addContents(mHeaderCount, privacyList);
                mHeaderCount += privacyList.size();
                mStream.removeOnContentChangedListener(this);
                mStream.notifyNewHeaderCount(mHeaderCount);
            } else if (TextUtils.isEmpty(mCreatorModel.get(CreatorProperties.URL_KEY))
                    || TextUtils.isEmpty(mCreatorModel.get(CreatorProperties.TITLE_KEY))) {
                // If there is an error, hide the profile section if either the creator URL or
                // creator title is unavailable.
                mContentManager.removeContents(0, 1);
                mHeaderCount -= 1;
                mStream.removeOnContentChangedListener(this);
                mStream.notifyNewHeaderCount(mHeaderCount);
            }
        }
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
            FaviconHelper.FaviconImageCallback imageCallback =
                    (bitmap, iconUrl) -> {
                        Drawable drawable;
                        if (bitmap != null) {
                            drawable =
                                    FaviconUtils.createRoundedBitmapDrawable(
                                            mContext.getResources(), bitmap);
                        } else {
                            drawable =
                                    UiUtils.getTintedDrawable(
                                            mContext,
                                            R.drawable.ic_globe_24dp,
                                            R.color.default_icon_color_tint_list);
                        }

                        callback.onResult(drawable);
                    };

            mFaviconHelper.getLocalFaviconImageForURL(profile, url, mFaviconSize, imageCallback);
        }
    }
}
