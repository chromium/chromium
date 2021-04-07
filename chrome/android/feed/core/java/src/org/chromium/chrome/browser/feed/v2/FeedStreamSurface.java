// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.animation.ObjectAnimator;
import android.animation.PropertyValuesHolder;
import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewParent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemAnimator.ItemAnimatorFinishedListener;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.VideoPreviewsType;
import org.chromium.chrome.browser.feed.shared.ScrollTracker;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.suggestions.NavigationRecorder;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.chrome.browser.xsurface.SurfaceScopeDependencyProvider;
import org.chromium.chrome.browser.xsurface.SurfaceScopeDependencyProvider.AutoplayPreference;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.feed.proto.FeedUiProto.SharedState;
import org.chromium.components.feed.proto.FeedUiProto.Slice;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate.SliceUpdate;
import org.chromium.components.feed.proto.FeedUiProto.ZeroStateSlice;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * Bridge class that lets Android code access native code for feed related functionalities.
 *
 * Created once for each StreamSurfaceMediator corresponding to each NTP/start surface.
 */
@JNINamespace("feed::android")
public class FeedStreamSurface
        implements SurfaceActionsHandler, FeedActionsHandler, DisplayAndroidObserver {
    private static final String TAG = "FeedStreamSurface";

    private static final int SNACKBAR_DURATION_MS_SHORT = 4000;
    private static final int SNACKBAR_DURATION_MS_LONG = 10000;

    @VisibleForTesting
    static final String FEEDBACK_REPORT_TYPE =
            "com.google.chrome.feed.USER_INITIATED_FEEDBACK_REPORT";
    @VisibleForTesting
    static final String XSURFACE_CARD_URL = "Card URL";
    // For testing some functionality in the public APK.
    @VisibleForTesting
    public static boolean sRequestContentWithoutRendererForTesting;

    private final long mNativeFeedStreamSurface;
    private final FeedListContentManager mContentManager;
    private final SurfaceScope mSurfaceScope;
    @VisibleForTesting
    RecyclerView mRootView;
    private final HybridListRenderer mHybridListRenderer;
    private final SnackbarManager mSnackbarManager;
    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;
    @Nullable
    private FeedSliceViewTracker mSliceViewTracker;
    private final NativePageNavigationDelegate mPageNavigationDelegate;
    private final HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    private final ScrollReporter mScrollReporter = new ScrollReporter();
    private final ObserverList<ContentChangedListener> mContentChangedListeners =
            new ObserverList<ContentChangedListener>();
    private final RecyclerViewAnimationFinishDetector mRecyclerViewAnimationFinishDetector =
            new RecyclerViewAnimationFinishDetector();
    // True after onSurfaceOpened(), and before onSurfaceClosed().
    private boolean mOpened;
    private boolean mStreamContentVisible;
    private boolean mStreamVisible;
    private int mHeaderCount;
    private BottomSheetContent mBottomSheetContent;
    // If the bottom sheet was opened in response to an action on a slice, this is the slice ID.
    private String mBottomSheetOriginatingSliceId;
    private final int mLoadMoreTriggerLookahead;
    private boolean mIsLoadingMoreContent;
    private boolean mIsPlaceholderShown;
    // TabSupplier for the current tab to share.
    private final ShareHelperWrapper mShareHelper;
    private final DisplayAndroid mDisplayAndroid;

    private static ProcessScope sXSurfaceProcessScope;

    // This must match the FeedSendFeedbackType enum in enums.xml.
    public @interface FeedFeedbackType {
        int FEEDBACK_TAPPED_ON_CARD = 0;
        int FEEDBACK_TAPPED_ON_PAGE = 1;
        int NUM_ENTRIES = 2;
    }

    // We avoid attaching surfaces until after |startup()| is called. This ensures that
    // the correct sign-in state is used if attaching the surface triggers a fetch.
    private static boolean sStartupCalled;
    private static boolean sSetServiceBridgeDelegate;
    // Tracks all the instances of FeedStreamSurface.
    @VisibleForTesting
    static HashSet<FeedStreamSurface> sSurfaces;

    /**
     * Initializes the FeedServiceBridge. We do this once at startup, either in startup(), or in
     * FeedStreamSurface's constructor, whichever comes first.
     */
    private static void initServiceBridge() {
        if (sSetServiceBridgeDelegate) return;
        sSetServiceBridgeDelegate = true;
        FeedServiceBridge.setDelegate(new FeedServiceBridgeDelegateImpl());
    }

    public static void startup() {
        if (sStartupCalled) return;
        sStartupCalled = true;
        initServiceBridge();
        FeedServiceBridge.startup();
        if (sSurfaces != null) {
            for (FeedStreamSurface surface : sSurfaces) {
                surface.updateSurfaceOpenState();
            }
        }
    }

    // Only called for cleanup during testing.
    @VisibleForTesting
    static void shutdownForTesting() {
        sStartupCalled = false;
        sSurfaces = null;
        sXSurfaceProcessScope = null;
    }

    private static void trackSurface(FeedStreamSurface surface) {
        if (sSurfaces == null) {
            sSurfaces = new HashSet<FeedStreamSurface>();
        }
        sSurfaces.add(surface);
    }

    private static void untrackSurface(FeedStreamSurface surface) {
        if (sSurfaces != null) {
            sSurfaces.remove(surface);
        }
    }

    /**
     *  Clear all the data related to all surfaces.
     */
    public static void clearAll() {
        if (sSurfaces == null) return;

        ArrayList<FeedStreamSurface> openSurfaces = new ArrayList<FeedStreamSurface>();
        for (FeedStreamSurface surface : sSurfaces) {
            if (surface.isOpened()) openSurfaces.add(surface);
        }
        for (FeedStreamSurface surface : openSurfaces) {
            surface.onSurfaceClosed();
        }

        ProcessScope processScope = FeedServiceBridge.xSurfaceProcessScope();
        if (processScope != null) {
            processScope.resetAccount();
        }

        for (FeedStreamSurface surface : openSurfaces) {
            surface.updateSurfaceOpenState();
        }
    }

    /**
     * Provides a wrapper around sharing methods.
     *
     * Makes it easier to test.
     */
    public static class ShareHelperWrapper {
        private WindowAndroid mWindowAndroid;
        private Supplier<ShareDelegate> mShareDelegateSupplier;
        public ShareHelperWrapper(
                WindowAndroid windowAndroid, Supplier<ShareDelegate> shareDelegateSupplier) {
            mWindowAndroid = windowAndroid;
            mShareDelegateSupplier = shareDelegateSupplier;
        }

        /**
         * Shares a url and title from Chrome to another app.
         * Brings up the share sheet.
         */
        public void share(String url, String title) {
            ShareParams params = new ShareParams.Builder(mWindowAndroid, title, url).build();
            mShareDelegateSupplier.get().share(
                    params, new ChromeShareExtras.Builder().build(), ShareOrigin.FEED);
        }
    }

    /**
     * Provides activity and darkmode context for a single surface.
     */
    private class FeedSurfaceScopeDependencyProvider implements SurfaceScopeDependencyProvider {
        final Activity mActivity;
        final Context mActivityContext;
        final boolean mDarkMode;

        FeedSurfaceScopeDependencyProvider(
                Activity activity, Context activityContext, boolean darkMode) {
            mActivity = activity;
            mActivityContext =
                    FeedProcessScopeDependencyProvider.createFeedContext(activityContext);
            mDarkMode = darkMode;
        }

        @Override
        public Activity getActivity() {
            return mActivity;
        }

        @Override
        public Context getActivityContext() {
            return mActivityContext;
        }

        @Override
        public boolean isDarkModeEnabled() {
            return mDarkMode;
        }

        @Override
        public boolean isActivityLoggingEnabled() {
            return FeedStreamSurfaceJni.get().isActivityLoggingEnabled(
                    mNativeFeedStreamSurface, FeedStreamSurface.this);
        }

        @Override
        public String getAccountName() {
            // Don't return account name if there's a signed-out session ID.
            if (!getSignedOutSessionId().isEmpty()) {
                return "";
            }
            assert ThreadUtils.runningOnUiThread();
            CoreAccountInfo primaryAccount =
                    IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
            return (primaryAccount == null) ? "" : primaryAccount.getEmail();
        }

        @Override
        public int[] getExperimentIds() {
            assert ThreadUtils.runningOnUiThread();
            return FeedStreamSurfaceJni.get().getExperimentIds();
        }

        @Override
        public String getClientInstanceId() {
            // Don't return client instance id if there's a signed-out session ID.
            if (!getSignedOutSessionId().isEmpty()) {
                return "";
            }
            assert ThreadUtils.runningOnUiThread();
            return FeedServiceBridge.getClientInstanceId();
        }

        @Override
        public String getSignedOutSessionId() {
            assert ThreadUtils.runningOnUiThread();
            return FeedStreamSurfaceJni.get().getSessionId(
                    mNativeFeedStreamSurface, FeedStreamSurface.this);
        }

        @Override
        public AutoplayPreference getAutoplayPreference() {
            assert ThreadUtils.runningOnUiThread();
            @VideoPreviewsType
            int videoPreviewsType = FeedServiceBridge.getVideoPreviewsTypePreference();
            switch (videoPreviewsType) {
                case VideoPreviewsType.NEVER:
                    return AutoplayPreference.AUTOPLAY_DISABLED;
                case VideoPreviewsType.WIFI_AND_MOBILE_DATA:
                    return AutoplayPreference.AUTOPLAY_ON_WIFI_AND_MOBILE_DATA;
                case VideoPreviewsType.WIFI:
                default:
                    return AutoplayPreference.AUTOPLAY_ON_WIFI_ONLY;
            }
        }
    }

    /**
     * A {@link TabObserver} that observes navigation related events that originate from Feed
     * interactions. Calls reportPageLoaded when navigation completes.
     */
    private class FeedTabNavigationObserver extends EmptyTabObserver {
        private final boolean mInNewTab;

        FeedTabNavigationObserver(boolean inNewTab) {
            mInNewTab = inNewTab;
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            // TODO(jianli): onPageLoadFinished is called on successful load, and if a user manually
            // stops the page load. We should only capture successful page loads.
            FeedStreamSurfaceJni.get().reportPageLoaded(
                    mNativeFeedStreamSurface, FeedStreamSurface.this, mInNewTab);
            tab.removeObserver(this);
        }

        @Override
        public void onPageLoadFailed(Tab tab, int errorCode) {
            tab.removeObserver(this);
        }

        @Override
        public void onCrash(Tab tab) {
            tab.removeObserver(this);
        }

        @Override
        public void onDestroyed(Tab tab) {
            tab.removeObserver(this);
        }
    }

    /**
     * Creates a {@link FeedStreamSurface} for creating native side bridge to access native feed
     * client implementation.
     */
    public FeedStreamSurface(Activity activity, boolean isBackgroundDark,
            SnackbarManager snackbarManager, NativePageNavigationDelegate pageNavigationDelegate,
            BottomSheetController bottomSheetController,
            HelpAndFeedbackLauncher helpAndFeedbackLauncher, boolean isPlaceholderShown,
            ShareHelperWrapper shareHelper, DisplayAndroid displayAndroid) {
        initServiceBridge();
        mNativeFeedStreamSurface = FeedStreamSurfaceJni.get().init(FeedStreamSurface.this);
        mSnackbarManager = snackbarManager;
        mActivity = activity;
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;

        mPageNavigationDelegate = pageNavigationDelegate;
        mBottomSheetController = bottomSheetController;
        mLoadMoreTriggerLookahead = FeedServiceBridge.getLoadMoreTriggerLookahead();

        mContentManager = new FeedListContentManager(this, this);

        mIsPlaceholderShown = isPlaceholderShown;
        mShareHelper = shareHelper;
        mDisplayAndroid = displayAndroid;

        Context context = new ContextThemeWrapper(
                activity, (isBackgroundDark ? R.style.Dark : R.style.Light));

        ProcessScope processScope = FeedServiceBridge.xSurfaceProcessScope();
        if (processScope != null) {
            mSurfaceScope = processScope.obtainSurfaceScope(
                    new FeedSurfaceScopeDependencyProvider(activity, context, isBackgroundDark));
        } else {
            mSurfaceScope = null;
        }

        if (mSurfaceScope != null) {
            mHybridListRenderer = mSurfaceScope.provideListRenderer();
        } else {
            mHybridListRenderer = new NativeViewListRenderer(context);
        }

        if (mHybridListRenderer != null) {
            // XSurface returns a View, but it should be a RecyclerView.
            mRootView = (RecyclerView) mHybridListRenderer.bind(mContentManager);

            mSliceViewTracker =
                    new FeedSliceViewTracker(mRootView, mContentManager, new ViewTrackerObserver());
        } else {
            mRootView = null;
        }

        // Attach as an observer of window events so we can consume window rotation events.
        if (mDisplayAndroid != null) {
            mDisplayAndroid.addObserver(this);
        }

        trackSurface(this);
    }

    /**
     * Performs all necessary cleanups.
     */
    public void destroy() {
        if (mOpened) onSurfaceClosed();
        untrackSurface(this);
        if (mSliceViewTracker != null) {
            mSliceViewTracker.destroy();
            mSliceViewTracker = null;
        }
        mHybridListRenderer.unbind();
        if (mDisplayAndroid != null) {
            mDisplayAndroid.removeObserver(this);
        }
    }

    /**
     * Puts a list of header views at the beginning.
     */
    public void setHeaderViews(List<View> headerViews) {
        ArrayList<FeedListContentManager.FeedContent> newContentList =
                new ArrayList<FeedListContentManager.FeedContent>();

        // First add new header contents. Some of them may appear in the existing list.
        for (int i = 0; i < headerViews.size(); ++i) {
            View view = headerViews.get(i);
            String key = "Header" + view.hashCode();
            FeedListContentManager.NativeViewContent headerContent =
                    new FeedListContentManager.NativeViewContent(key, view);
            newContentList.add(headerContent);
        }

        // Then add all existing feed stream contents.
        for (int i = mHeaderCount; i < mContentManager.getItemCount(); ++i) {
            newContentList.add(mContentManager.getContent(i));
        }

        updateContentsInPlace(newContentList);

        mHeaderCount = headerViews.size();
    }

    /**
     * @return The android {@link View} that the surface is supposed to show.
     */
    public View getView() {
        return mRootView;
    }

    /**
     * Attempts to load more content if it can be triggered.
     *
     * <p>This method uses the default or Finch configured load more lookahead trigger.
     *
     * @return true if loading more content can be triggered.
     */
    boolean maybeLoadMore() {
        return maybeLoadMore(mLoadMoreTriggerLookahead);
    }

    /**
     * Attempts to load more content if it can be triggered.
     * @param lookaheadTrigger The threshold of off-screen cards below which the feed should attempt
     *         to load more content. I.e., if there are fewer than |lookaheadTrigger| cards left to
     *         show the user, then the feed should load more cards.
     * @return true if loading more content can be triggered.
     */
    private boolean maybeLoadMore(int lookaheadTrigger) {
        // Checks if loading more can be triggered.
        boolean canLoadMore = false;
        LinearLayoutManager layoutManager = (LinearLayoutManager) mRootView.getLayoutManager();
        if (layoutManager == null) {
            return false;
        }
        int totalItemCount = layoutManager.getItemCount();
        int lastVisibleItem = layoutManager.findLastVisibleItemPosition();
        if (totalItemCount - lastVisibleItem > lookaheadTrigger) {
            return false;
        }

        // Starts to load more content if not yet.
        if (!mIsLoadingMoreContent) {
            mIsLoadingMoreContent = true;
            // The native loadMore() call may immediately result in onStreamUpdated(), which can
            // result in a crash if maybeLoadMore() is being called in response to certain events.
            // Use postTask to avoid this.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    ()
                            -> FeedStreamSurfaceJni.get().loadMore(mNativeFeedStreamSurface,
                                    FeedStreamSurface.this,
                                    (Boolean success) -> { mIsLoadingMoreContent = false; }));
        }

        return true;
    }

    @VisibleForTesting
    FeedListContentManager getFeedListContentManagerForTesting() {
        return mContentManager;
    }

    /**
     * Called when the stream update content is available. The content will get passed to UI
     */
    @CalledByNative
    void onStreamUpdated(byte[] data) {
        // There should be no updates while the surface is closed. If the surface was recently
        // closed, just ignore these.
        if (!mOpened) return;
        StreamUpdate streamUpdate;
        try {
            streamUpdate = StreamUpdate.parseFrom(data);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            Log.wtf(TAG, "Unable to parse StreamUpdate proto data", e);
            return;
        }

        // Update using shared states.
        for (SharedState state : streamUpdate.getNewSharedStatesList()) {
            mHybridListRenderer.update(state.getXsurfaceSharedState().toByteArray());
        }

        // Builds the new list containing:
        // * existing headers
        // * both new and existing contents
        ArrayList<FeedListContentManager.FeedContent> newContentList =
                new ArrayList<FeedListContentManager.FeedContent>();
        for (int i = 0; i < mHeaderCount; ++i) {
            newContentList.add(mContentManager.getContent(i));
        }
        for (SliceUpdate sliceUpdate : streamUpdate.getUpdatedSlicesList()) {
            if (sliceUpdate.hasSlice()) {
                FeedListContentManager.FeedContent content =
                        createContentFromSlice(sliceUpdate.getSlice());
                if (content != null) {
                    newContentList.add(content);
                }
            } else {
                String existingSliceId = sliceUpdate.getSliceId();
                int position = mContentManager.findContentPositionByKey(existingSliceId);
                if (position != -1) {
                    newContentList.add(mContentManager.getContent(position));
                }
            }
        }

        updateContentsInPlace(newContentList);

        // If all of the cards fit on the screen, load more content. The view
        // may not be scrollable, preventing the user from otherwise triggering
        // load more.
        maybeLoadMore(/*lookaheadTrigger=*/0);
    }

    @CalledByNative
    void replaceDataStoreEntry(String key, byte[] data) {
        if (mSurfaceScope != null) mSurfaceScope.replaceDataStoreEntry(key, data);
    }

    @CalledByNative
    void removeDataStoreEntry(String key) {
        if (mSurfaceScope != null) mSurfaceScope.removeDataStoreEntry(key);
    }

    private void updateContentsInPlace(
            ArrayList<FeedListContentManager.FeedContent> newContentList) {
        boolean hasContentChange = false;

        // 1) Builds the hash set based on keys of new contents.
        HashSet<String> newContentKeySet = new HashSet<String>();
        for (int i = 0; i < newContentList.size(); ++i) {
            hasContentChange = true;
            newContentKeySet.add(newContentList.get(i).getKey());
        }

        // 2) Builds the hash map of existing content list for fast look up by key.
        HashMap<String, FeedListContentManager.FeedContent> existingContentMap =
                new HashMap<String, FeedListContentManager.FeedContent>();
        for (int i = 0; i < mContentManager.getItemCount(); ++i) {
            FeedListContentManager.FeedContent content = mContentManager.getContent(i);
            existingContentMap.put(content.getKey(), content);
        }

        // 3) Removes those existing contents that do not appear in the new list.
        for (int i = mContentManager.getItemCount() - 1; i >= 0; --i) {
            String key = mContentManager.getContent(i).getKey();
            if (!newContentKeySet.contains(key)) {
                hasContentChange = true;
                mContentManager.removeContents(i, 1);
                existingContentMap.remove(key);
            }
        }

        // 4) Iterates through the new list to add the new content or move the existing content
        //    if needed.
        int i = 0;
        while (i < newContentList.size()) {
            FeedListContentManager.FeedContent content = newContentList.get(i);

            // If this is an existing content, moves it to new position.
            if (existingContentMap.containsKey(content.getKey())) {
                hasContentChange = true;
                mContentManager.moveContent(
                        mContentManager.findContentPositionByKey(content.getKey()), i);
                ++i;
                continue;
            }

            // Otherwise, this is new content. Add it together with all adjacent new contents.
            int startIndex = i++;
            while (i < newContentList.size()
                    && !existingContentMap.containsKey(newContentList.get(i).getKey())) {
                ++i;
            }
            hasContentChange = true;
            mContentManager.addContents(startIndex, newContentList.subList(startIndex, i));
        }

        if (hasContentChange) {
            mRecyclerViewAnimationFinishDetector.asyncWait();
        }
    }

    private void notifyContentChanged() {
        for (ContentChangedListener listener : mContentChangedListeners) {
            // For Feed v2, we only need to report if the content has changed. All other callbacks
            // are not used at this point.
            listener.onContentChanged();
        }
    }

    private FeedListContentManager.FeedContent createContentFromSlice(Slice slice) {
        String sliceId = slice.getSliceId();
        if (slice.hasXsurfaceSlice()) {
            return new FeedListContentManager.ExternalViewContent(
                    sliceId, slice.getXsurfaceSlice().getXsurfaceFrame().toByteArray());
        } else if (slice.hasLoadingSpinnerSlice()) {
            // If the placeholder is shown, spinner is not needed.
            if (mIsPlaceholderShown) {
                return null;
            }
            return new FeedListContentManager.NativeViewContent(sliceId, R.layout.feed_spinner);
        }
        assert slice.hasZeroStateSlice();
        if (slice.getZeroStateSlice().getType() == ZeroStateSlice.Type.CANT_REFRESH) {
            return new FeedListContentManager.NativeViewContent(sliceId, R.layout.no_connection);
        }
        assert slice.getZeroStateSlice().getType() == ZeroStateSlice.Type.NO_CARDS_AVAILABLE;
        return new FeedListContentManager.NativeViewContent(sliceId, R.layout.no_content_v2);
    }

    /**
     * Returns the immediate child of parentView which contains descendentView.
     * If descendentView is not in parentView's view heirarchy, this returns null.
     * Note that the returned view may be descendentView, or descendentView.getParent(),
     * or descendentView.getParent().getParent(), etc...
     */
    View findChildViewContainingDescendent(View parentView, View descendentView) {
        if (parentView == null || descendentView == null) return null;
        // Find the direct child of parentView which owns view.
        if (parentView == descendentView.getParent()) {
            return descendentView;
        } else {
            // One of the view's ancestors might be the child.
            ViewParent p = descendentView.getParent();
            while (true) {
                if (p == null) {
                    return null;
                }
                if (p.getParent() == parentView) {
                    if (p instanceof View) return (View) p;
                    return null;
                }
                p = p.getParent();
            }
        }
    }

    @VisibleForTesting
    String getSliceIdFromView(View view) {
        View childOfRoot = findChildViewContainingDescendent(mRootView, view);

        if (childOfRoot != null) {
            // View is a child of the recycler view, find slice using the index.
            int position = mRootView.getChildAdapterPosition(childOfRoot);
            if (position >= 0 && position < mContentManager.getItemCount()) {
                return mContentManager.getContent(position).getKey();
            }
        } else if (mBottomSheetContent != null
                && findChildViewContainingDescendent(mBottomSheetContent.getContentView(), view)
                        != null) {
            // View is a child of the bottom sheet, return slice associated with the bottom sheet.
            return mBottomSheetOriginatingSliceId;
        }
        return "";
    }

    @Override
    public void navigateTab(String url, View actionSourceView) {
        assert ThreadUtils.runningOnUiThread();
        FeedStreamSurfaceJni.get().reportOpenAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, getSliceIdFromView(actionSourceView));
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);

        openUrl(url, WindowOpenDisposition.CURRENT_TAB);

        // Attempts to load more content if needed.
        maybeLoadMore();
    }

    @Override
    public void navigateNewTab(String url, View actionSourceView) {
        assert ThreadUtils.runningOnUiThread();
        FeedStreamSurfaceJni.get().reportOpenInNewTabAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, getSliceIdFromView(actionSourceView));
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);

        openUrl(url, WindowOpenDisposition.NEW_BACKGROUND_TAB);

        // Attempts to load more content if needed.
        maybeLoadMore();
    }

    @Override
    public void navigateIncognitoTab(String url) {
        assert ThreadUtils.runningOnUiThread();
        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, FeedUserActionType.TAPPED_OPEN_IN_NEW_INCOGNITO_TAB);
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);

        openUrl(url, WindowOpenDisposition.OFF_THE_RECORD);

        // Attempts to load more content if needed.
        maybeLoadMore();
    }

    @Override
    public void downloadLink(String url) {
        assert ThreadUtils.runningOnUiThread();
        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, FeedUserActionType.TAPPED_DOWNLOAD);
        RequestCoordinatorBridge.getForProfile(Profile.getLastUsedRegularProfile())
                .savePageLater(
                        url, OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE, true /* user requested*/);
    }

    @Override
    public void showBottomSheet(View view, View actionSourceView) {
        assert ThreadUtils.runningOnUiThread();
        dismissBottomSheet();

        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, FeedUserActionType.OPENED_CONTEXT_MENU);

        // Make a sheetContent with the view.
        mBottomSheetContent = new CardMenuBottomSheetContent(view);
        mBottomSheetOriginatingSliceId = getSliceIdFromView(actionSourceView);
        mBottomSheetController.requestShowContent(mBottomSheetContent, true);
    }

    @Override
    public void dismissBottomSheet() {
        assert ThreadUtils.runningOnUiThread();
        if (mBottomSheetContent != null) {
            mBottomSheetController.hideContent(mBottomSheetContent, true);
        }
        mBottomSheetContent = null;
        mBottomSheetOriginatingSliceId = null;
    }

    public void recordActionManageActivity() {
        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, FeedUserActionType.TAPPED_MANAGE_ACTIVITY);
    }

    public void recordActionManageInterests() {
        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, FeedUserActionType.TAPPED_MANAGE_INTERESTS);
    }

    public void recordActionManageReactions() {
        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, FeedUserActionType.TAPPED_MANAGE_REACTIONS);
    }

    public void recordActionLearnMore() {
        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, FeedUserActionType.TAPPED_LEARN_MORE);
    }

    @Override
    public void loadMore() {
        // TODO(jianli): Remove this from FeedActionsHandler interface.
    }

    @Override
    public void processThereAndBackAgainData(byte[] data) {
        processThereAndBackAgainData(data, null);
    }

    @Override
    public void processThereAndBackAgainData(byte[] data, @Nullable View actionSourceView) {
        assert ThreadUtils.runningOnUiThread();
        FeedStreamSurfaceJni.get().processThereAndBackAgain(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    @Override
    public void processViewAction(byte[] data) {
        FeedStreamSurfaceJni.get().processViewAction(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    @Override
    public void sendFeedback(Map<String, String> productSpecificDataMap) {
        assert ThreadUtils.runningOnUiThread();
        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this, FeedUserActionType.TAPPED_SEND_FEEDBACK);

        // Make sure the bottom sheet is dismissed before we take a snapshot.
        dismissBottomSheet();

        Profile profile = Profile.getLastUsedRegularProfile();
        if (profile == null) {
            return;
        }

        String url = productSpecificDataMap.get(XSURFACE_CARD_URL);

        Map<String, String> feedContext = convertNameFormat(productSpecificDataMap);

        // FEEDBACK_REPORT_TYPE: Reports for Chrome mobile must have a contextTag of the form
        // com.chrome.feed.USER_INITIATED_FEEDBACK_REPORT, or they will be discarded for not
        // matching an allow list rule.
        mHelpAndFeedbackLauncher.showFeedback(
                mActivity, profile, url, FEEDBACK_REPORT_TYPE, feedContext);
    }

    // Since the XSurface client strings are slightly different than the Feed strings, convert the
    // name from the XSurface format to the format that can be handled by the feedback system.  Any
    // new strings that are added on the XSurface side will need a code change here, and adding the
    // PSD to the allow list.
    private Map<String, String> convertNameFormat(Map<String, String> xSurfaceMap) {
        Map<String, String> feedbackNameConversionMap = new HashMap<>();
        feedbackNameConversionMap.put("Card URL", "CardUrl");
        feedbackNameConversionMap.put("Card Title", "CardTitle");
        feedbackNameConversionMap.put("Card Snippet", "CardSnippet");
        feedbackNameConversionMap.put("Card category", "CardCategory");
        feedbackNameConversionMap.put("Doc Creation Date", "DocCreationDate");

        // For each <name, value> entry in the input map, convert the name to the new name, and
        // write the new <name, value> pair into the output map.
        Map<String, String> feedbackMap = new HashMap<>();
        for (Map.Entry<String, String> entry : xSurfaceMap.entrySet()) {
            String newName = feedbackNameConversionMap.get(entry.getKey());
            if (newName != null) {
                feedbackMap.put(newName, entry.getValue());
            } else {
                Log.v(TAG, "Found an entry with no conversion available.");
                // We will put the entry into the map if untranslatable. It will be discarded
                // unless it matches an allow list on the server, though. This way we can choose
                // to allow it on the server if desired.
                feedbackMap.put(entry.getKey(), entry.getValue());
            }
        }

        return feedbackMap;
    }

    @Override
    public int requestDismissal(byte[] data) {
        assert ThreadUtils.runningOnUiThread();
        return FeedStreamSurfaceJni.get().executeEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    @Override
    public void commitDismissal(int changeId) {
        assert ThreadUtils.runningOnUiThread();
        FeedStreamSurfaceJni.get().commitEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);

        // Attempts to load more content if needed.
        maybeLoadMore();
    }

    @Override
    public void discardDismissal(int changeId) {
        assert ThreadUtils.runningOnUiThread();
        FeedStreamSurfaceJni.get().discardEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);
    }

    @Override
    public void showSnackbar(String text, String actionLabel,
            FeedActionsHandler.SnackbarDuration duration,
            FeedActionsHandler.SnackbarController controller) {
        assert ThreadUtils.runningOnUiThread();
        int durationMs = SNACKBAR_DURATION_MS_SHORT;
        if (duration == FeedActionsHandler.SnackbarDuration.LONG) {
            durationMs = SNACKBAR_DURATION_MS_LONG;
        }

        mSnackbarManager.showSnackbar(
                Snackbar.make(text,
                                new SnackbarManager.SnackbarController() {
                                    @Override
                                    public void onAction(Object actionData) {
                                        controller.onAction();
                                    }
                                    @Override
                                    public void onDismissNoAction(Object actionData) {
                                        controller.onDismissNoAction();
                                    }
                                },
                                Snackbar.TYPE_ACTION, Snackbar.UMA_FEED_NTP_STREAM)
                        .setAction(actionLabel, /*actionData=*/null)
                        .setDuration(durationMs));
    }

    @Override
    public void share(String url, String title) {
        mShareHelper.share(url, title);
    }

    /**
     * Informs whether or not feed content should be shown.
     */
    public void setStreamContentVisibility(boolean visible) {
        if (mStreamContentVisible == visible) return;
        mStreamContentVisible = visible;
        updateSurfaceOpenState();
    }

    public void toggledArticlesListVisible(boolean visible) {
        FeedStreamSurfaceJni.get().reportOtherUserAction(mNativeFeedStreamSurface,
                FeedStreamSurface.this,
                visible ? FeedUserActionType.TAPPED_TURN_ON : FeedUserActionType.TAPPED_TURN_OFF);
    }

    /**
     * Informs FeedStreamSurface of the visibility of its parent stream.
     */
    public void setStreamVisibility(boolean visible) {
        if (mStreamVisible == visible) return;
        mStreamVisible = visible;
        updateSurfaceOpenState();
    }

    private void updateSurfaceOpenState() {
        boolean shouldOpen = sStartupCalled && mStreamContentVisible && mStreamVisible;
        if (shouldOpen == mOpened) return;
        if (shouldOpen) {
            onSurfaceOpened();
        } else {
            onSurfaceClosed();
        }
    }

    /**
     * Called when the surface is considered opened. This happens when the feed should be visible
     * and enabled on the screen.
     */
    private void onSurfaceOpened() {
        assert (!mOpened);
        assert (sStartupCalled);
        assert (mStreamContentVisible);
        // No feed content should exist.
        assert (mContentManager.getItemCount() == mHeaderCount);

        mOpened = true;
        // Don't ask native to load content if there's no way to render it.
        if (mSurfaceScope != null || sRequestContentWithoutRendererForTesting) {
            FeedStreamSurfaceJni.get().surfaceOpened(
                    mNativeFeedStreamSurface, FeedStreamSurface.this);
        }
        mHybridListRenderer.onSurfaceOpened();
    }

    /**
     * Informs that the surface is closed.
     */
    private void onSurfaceClosed() {
        assert (mOpened);
        assert (sStartupCalled);
        // Let the hybrid list renderer know that the surface has closed, so it doesn't
        // interpret the removal of contents as related to actions otherwise initiated by
        // the user.
        mHybridListRenderer.onSurfaceClosed();

        // Remove Feed content from the content manager.
        int feedCount = mContentManager.getItemCount() - mHeaderCount;
        if (feedCount > 0) {
            mContentManager.removeContents(mHeaderCount, feedCount);
            mRecyclerViewAnimationFinishDetector.asyncWait();
        }

        mScrollReporter.onUnbind();
        mSliceViewTracker.clear();
        if (mSurfaceScope != null || sRequestContentWithoutRendererForTesting) {
            FeedStreamSurfaceJni.get().surfaceClosed(
                    mNativeFeedStreamSurface, FeedStreamSurface.this);
        }
        mOpened = false;
    }

    public boolean isOpened() {
        return mOpened;
    }

    private void openUrl(String url, int disposition) {
        LoadUrlParams params = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        params.setReferrer(
                new Referrer(SuggestionsConfig.getReferrerUrl(ChromeFeatureList.INTEREST_FEED_V2),
                        // WARNING: ReferrerPolicy.ALWAYS is assumed by other Chrome code for NTP
                        // tiles to set consider_for_ntp_most_visited.
                        ReferrerPolicy.ALWAYS));
        Tab tab = mPageNavigationDelegate.openUrl(disposition, params);

        boolean inNewTab = (disposition == WindowOpenDisposition.NEW_BACKGROUND_TAB
                || disposition == WindowOpenDisposition.OFF_THE_RECORD);

        if (tab != null) {
            tab.addObserver(new FeedTabNavigationObserver(inNewTab));
            NavigationRecorder.record(tab,
                    visitData -> FeedServiceBridge.reportOpenVisitComplete(visitData.duration));
        }
    }

    public void addContentChangedListener(ContentChangedListener listener) {
        mContentChangedListeners.addObserver(listener);
    }

    public void removeContentChangedListener(ContentChangedListener listener) {
        mContentChangedListeners.removeObserver(listener);
    }

    // Called when the stream is scrolled.
    void streamScrolled(int dx, int dy) {
        FeedStreamSurfaceJni.get().reportStreamScrollStart(
                mNativeFeedStreamSurface, FeedStreamSurface.this);
        mScrollReporter.trackScroll(dx, dy);
    }

    boolean isPlaceholderShown() {
        return mIsPlaceholderShown;
    }

    /**
     * Feed v2's background is set to be transparent in {@link FeedSurfaceCoordinator#createStream}
     * if the Feed placeholder is shown. After first batch of articles are loaded, set recyclerView
     * back to non-transparent. Since Feed v2 doesn't have fade-in animation, we add a fade-in
     * animation for Feed background to make the transition smooth.
     */
    void hidePlaceholder() {
        if (!mIsPlaceholderShown) {
            return;
        }
        ObjectAnimator animator = ObjectAnimator.ofPropertyValuesHolder(
                mRootView.getBackground(), PropertyValuesHolder.ofInt("alpha", 255));
        animator.setTarget(mRootView.getBackground());
        animator.setDuration(mRootView.getItemAnimator().getAddDuration())
                .setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        animator.start();
        mIsPlaceholderShown = false;
    }

    // Detects animation finishes in RecyclerView.
    // https://stackoverflow.com/questions/33710605/detect-animation-finish-in-androids-recyclerview
    private class RecyclerViewAnimationFinishDetector implements ItemAnimatorFinishedListener {
        private boolean mWaitingStarted;

        /** Asynchronousy waits for the animation to finish. */
        public void asyncWait() {
            if (mWaitingStarted) {
                return;
            }
            mWaitingStarted = true;

            // The RecyclerView has not started animating yet, so post a message to the
            // message queue that will be run after the RecyclerView has started animating.
            new Handler().post(() -> { checkFinish(); });
        }

        private void checkFinish() {
            if (mRootView.isAnimating()) {
                // The RecyclerView is still animating, try again when the animation has finished.
                mRootView.getItemAnimator().isRunning(this);
                return;
            }

            // The RecyclerView has animated all it's views.
            onFinished();
        }

        private void onFinished() {
            mWaitingStarted = false;

            // This works around the bug that the out-of-screen toolbar is not brought back together
            // with the new tab page view when it slides down. This is because the RecyclerView
            // animation may not finish when content changed event is triggered and thus the new tab
            // page layout view may still be partially off screen.
            notifyContentChanged();
        }

        @Override
        public void onAnimationsFinished() {
            // There might still be more items that will be animated after this one.
            new Handler().post(() -> { checkFinish(); });
        }
    }

    // Ingests scroll events and reports scroll completion back to native.
    private class ScrollReporter extends ScrollTracker {
        @Override
        protected void onScrollEvent(int scrollAmount) {
            FeedStreamSurfaceJni.get().reportStreamScrolled(
                    mNativeFeedStreamSurface, FeedStreamSurface.this, scrollAmount);
        }
    }

    private class ViewTrackerObserver implements FeedSliceViewTracker.Observer {
        @Override
        public void sliceVisible(String sliceId) {
            FeedStreamSurfaceJni.get().reportSliceViewed(
                    mNativeFeedStreamSurface, FeedStreamSurface.this, sliceId);
        }
        @Override
        public void feedContentVisible() {
            FeedStreamSurfaceJni.get().reportFeedViewed(
                    mNativeFeedStreamSurface, FeedStreamSurface.this);
        }
    }

    // DisplayAndroidObserver methods.

    // If the device rotates, we dismiss the bottom sheet to avoid a bad interaction
    // between the XSurface client and the chrome bottom sheet.
    @Override
    public void onRotationChanged(int rotation) {
        dismissBottomSheet();
    }

    @NativeMethods
    interface Natives {
        long init(FeedStreamSurface caller);
        boolean isActivityLoggingEnabled(long nativeFeedStreamSurface, FeedStreamSurface caller);
        int[] getExperimentIds();
        String getSessionId(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void reportFeedViewed(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void reportSliceViewed(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        void reportPageLoaded(
                long nativeFeedStreamSurface, FeedStreamSurface caller, boolean inNewTab);
        void reportOpenAction(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        void reportOpenInNewTabAction(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        void reportOtherUserAction(long nativeFeedStreamSurface, FeedStreamSurface caller,
                @FeedUserActionType int userAction);
        void reportStreamScrolled(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int distanceDp);
        void reportStreamScrollStart(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void loadMore(
                long nativeFeedStreamSurface, FeedStreamSurface caller, Callback<Boolean> callback);
        void processThereAndBackAgain(
                long nativeFeedStreamSurface, FeedStreamSurface caller, byte[] data);
        void processViewAction(long nativeFeedStreamSurface, FeedStreamSurface caller, byte[] data);
        int executeEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, byte[] data);
        void commitEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int changeId);
        void discardEphemeralChange(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int changeId);
        void surfaceOpened(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void surfaceClosed(long nativeFeedStreamSurface, FeedStreamSurface caller);
    }
}
