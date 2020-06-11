// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.xsurface.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;
import org.chromium.chrome.browser.xsurface.SurfaceDependencyProvider;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.components.feed.proto.FeedUiProto.SharedState;
import org.chromium.components.feed.proto.FeedUiProto.Slice;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate.SliceUpdate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;

/**
 * Bridge class that lets Android code access native code for feed related functionalities.
 *
 * Created once for each StreamSurfaceMediator corresponding to each NTP/start surface.
 */
@JNINamespace("feed")
public class FeedStreamSurface implements SurfaceActionsHandler, FeedActionsHandler {
    private static final String TAG = "FeedStreamSurface";

    private static final int SNACKBAR_DURATION_MS_SHORT = 4000;
    private static final int SNACKBAR_DURATION_MS_LONG = 10000;

    private final long mNativeFeedStreamSurface;
    private final FeedListContentManager mContentManager;
    private final SurfaceScope mSurfaceScope;
    private final View mRootView;
    private final HybridListRenderer mHybridListRenderer;
    private final SnackbarManager mSnackbarManager;
    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;
    @Nullable
    private FeedSliceViewTracker mSliceViewTracker;
    private final NativePageNavigationDelegate mPageNavigationDelegate;

    private int mHeaderCount;

    private static ProcessScope sXSurfaceProcessScope;

    public static ProcessScope xSurfaceProcessScope() {
        if (sXSurfaceProcessScope == null) {
            sXSurfaceProcessScope =
                    AppHooks.get().getExternalSurfaceProcessScope(new SurfaceDependencyProvider() {
                        @Override
                        public Context getContext() {
                            return ContextUtils.getApplicationContext();
                        }
                    });
        }
        return sXSurfaceProcessScope;
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
        public void onPageLoadFinished(Tab tab, String url) {
            // TODO(jianli): onPageLoadFinished is called on successful load, and if a user manually
            // stops the page load. We should only capture successful page loads.
            FeedStreamSurfaceJni.get().reportPageLoaded(
                    mNativeFeedStreamSurface, FeedStreamSurface.this, url, mInNewTab);
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
    public FeedStreamSurface(Activity activity, SnackbarManager snackbarManager,
            NativePageNavigationDelegate pageNavigationDelegate,
            BottomSheetController bottomSheetController) {
        mNativeFeedStreamSurface = FeedStreamSurfaceJni.get().init(FeedStreamSurface.this);
        mSnackbarManager = snackbarManager;
        mActivity = activity;
        mPageNavigationDelegate = pageNavigationDelegate;
        mBottomSheetController = bottomSheetController;

        mContentManager = new FeedListContentManager(this, this);

        ProcessScope processScope = xSurfaceProcessScope();
        if (processScope != null) {
            mSurfaceScope = xSurfaceProcessScope().obtainSurfaceScope(mActivity);
        } else {
            mSurfaceScope = null;
        }

        if (mSurfaceScope != null) {
            mHybridListRenderer = mSurfaceScope.provideListRenderer();
        } else {
            mHybridListRenderer = new NativeViewListRenderer(mActivity);
        }

        if (mHybridListRenderer != null) {
            mRootView = mHybridListRenderer.bind(mContentManager);
            // XSurface returns a View, but it should be a RecyclerView.
            assert (mRootView instanceof RecyclerView);

            mSliceViewTracker = new FeedSliceViewTracker(
                    (RecyclerView) mRootView, mContentManager, (String sliceId) -> {
                        FeedStreamSurfaceJni.get().reportSliceViewed(
                                mNativeFeedStreamSurface, FeedStreamSurface.this, sliceId);
                    });
        } else {
            mRootView = null;
        }
    }

    /**
     * Performs all necessary cleanups.
     */
    public void destroy() {
        if (mSliceViewTracker != null) {
            mSliceViewTracker.destroy();
            mSliceViewTracker = null;
        }
        mHybridListRenderer.unbind();
        surfaceClosed();
    }

    /**
     * Puts a list of header views at the beginning.
     */
    public void setHeaderViews(List<View> headerViews) {
        if (mHeaderCount > 0) {
            mContentManager.removeContents(0, mHeaderCount);
            mHeaderCount = 0;
        }

        ArrayList<FeedListContentManager.FeedContent> headerContents =
                new ArrayList<FeedListContentManager.FeedContent>();
        for (int i = 0; i < headerViews.size(); ++i) {
            String key = "Header " + String.valueOf(i);
            FeedListContentManager.NativeViewContent headerContent =
                    new FeedListContentManager.NativeViewContent(key, headerViews.get(i));
            headerContents.add(headerContent);
        }
        mContentManager.addContents(0, headerContents);
        mHeaderCount = headerViews.size();
    }

    /**
     * @return The android {@link View} that the surface is supposed to show.
     */
    public View getView() {
        return mRootView;
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
        StreamUpdate streamUpdate;
        try {
            streamUpdate = StreamUpdate.parseFrom(data);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            Log.wtf(TAG, "Unable to parse StreamUpdate proto data", e);
            return;
        }

        // 0) Update using shared states.
        for (SharedState state : streamUpdate.getNewSharedStatesList()) {
            mHybridListRenderer.update(state.getXsurfaceSharedState().toByteArray());
        }

        // 1) Builds the hash map of existing content list for fast look up by slice id.
        HashMap<String, FeedListContentManager.FeedContent> existingContentMap =
                new HashMap<String, FeedListContentManager.FeedContent>();
        for (int i = mHeaderCount; i < mContentManager.getItemCount(); ++i) {
            FeedListContentManager.FeedContent content = mContentManager.getContent(i);
            existingContentMap.put(content.getKey(), content);
        }

        // 2) Builds the new list containing both new and existing contents.
        ArrayList<FeedListContentManager.FeedContent> newContentList =
                new ArrayList<FeedListContentManager.FeedContent>();
        HashSet<String> existingIdsInNewContentList = new HashSet<String>();
        for (SliceUpdate sliceUpdate : streamUpdate.getUpdatedSlicesList()) {
            if (sliceUpdate.hasSlice()) {
                newContentList.add(createContentFromSlice(sliceUpdate.getSlice()));
            } else {
                String existingSliceId = sliceUpdate.getSliceId();
                FeedListContentManager.FeedContent content =
                        existingContentMap.get(existingSliceId);
                if (content != null) {
                    newContentList.add(content);
                    existingIdsInNewContentList.add(existingSliceId);
                }
            }
        }

        // 3) Removes those contents that do not appear in the new list as the existing contents.
        //    Sometimes we may add new content with same id as the one in current list. In this
        //    case, we will remove it from current list and add it again later as new content.
        for (int i = mContentManager.getItemCount() - 1; i >= mHeaderCount; --i) {
            String id = mContentManager.getContent(i).getKey();
            if (!existingIdsInNewContentList.contains(id)) {
                mContentManager.removeContents(i, 1);
                existingContentMap.remove(id);
            }
        }

        // 4) Iterates through the new list to add the new content or move the existing content
        //    if needed.
        int i = 0;
        while (i < newContentList.size()) {
            FeedListContentManager.FeedContent content = newContentList.get(i);

            // If this is an existing content, moves it to new position.
            if (existingContentMap.containsKey(content.getKey())) {
                mContentManager.moveContent(
                        mContentManager.findContentPositionByKey(content.getKey()),
                        mHeaderCount + i);
                ++i;
                continue;
            }

            // Otherwise, this is new content. Add it together with all adjacent new contents.
            int startIndex = i++;
            while (i < newContentList.size()
                    && !existingContentMap.containsKey(newContentList.get(i).getKey())) {
                ++i;
            }
            mContentManager.addContents(
                    mHeaderCount + startIndex, newContentList.subList(startIndex, i));
        }
    }

    private FeedListContentManager.FeedContent createContentFromSlice(Slice slice) {
        String sliceId = slice.getSliceId();
        if (slice.hasXsurfaceSlice()) {
            return new FeedListContentManager.ExternalViewContent(
                    sliceId, slice.getXsurfaceSlice().getXsurfaceFrame().toByteArray());
        } else {
            // TODO(jianli): Create native view for ZeroStateSlice.
            TextView view = new TextView(ContextUtils.getApplicationContext());
            view.setText(sliceId);
            return new FeedListContentManager.NativeViewContent(sliceId, view);
        }
    }

    @Override
    public void navigateTab(String url) {
        openUrl(url, /*inNewTab=*/false);
    }

    @Override
    public void navigateNewTab(String url) {
        openUrl(url, /*inNewTab=*/true);
    }

    @Override
    public void loadMore() {
        FeedStreamSurfaceJni.get().loadMore(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    @Override
    public void processThereAndBackAgainData(byte[] data) {
        FeedStreamSurfaceJni.get().processThereAndBackAgain(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    @Override
    public int requestDismissal(byte[] data) {
        return FeedStreamSurfaceJni.get().executeEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, data);
    }

    @Override
    public void commitDismissal(int changeId) {
        FeedStreamSurfaceJni.get().commitEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);
    }

    @Override
    public void discardDismissal(int changeId) {
        FeedStreamSurfaceJni.get().discardEphemeralChange(
                mNativeFeedStreamSurface, FeedStreamSurface.this, changeId);
    }

    @Override
    public void showSnackbar(String text, String actionLabel,
            FeedActionsHandler.SnackbarDuration duration,
            FeedActionsHandler.SnackbarController controller) {
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
                        .setDuration(durationMs));
    }

    /**
     * Informs that the surface is opened. We can request the initial set of content now. Once
     * the content is available, onStreamUpdated will be called.
     */
    public void surfaceOpened() {
        FeedStreamSurfaceJni.get().surfaceOpened(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    /**
     * Informs that the surface is closed.
     */
    public void surfaceClosed() {
        int feedCount = mContentManager.getItemCount() - mHeaderCount;
        if (feedCount > 0) {
            mContentManager.removeContents(mHeaderCount, feedCount);
        }
        FeedStreamSurfaceJni.get().surfaceClosed(mNativeFeedStreamSurface, FeedStreamSurface.this);
    }

    private void openUrl(String url, boolean inNewTab) {
        LoadUrlParams params = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        params.setReferrer(
                new Referrer(SuggestionsConfig.getReferrerUrl(ChromeFeatureList.INTEREST_FEED_V2),
                        ReferrerPolicy.ALWAYS));
        Tab tab =
                mPageNavigationDelegate.openUrl(inNewTab ? WindowOpenDisposition.NEW_BACKGROUND_TAB
                                                         : WindowOpenDisposition.CURRENT_TAB,
                        params);

        FeedStreamSurfaceJni.get().reportNavigationStarted(
                mNativeFeedStreamSurface, FeedStreamSurface.this, url, inNewTab);
        tab.addObserver(new FeedTabNavigationObserver(inNewTab));
    }

    @NativeMethods
    interface Natives {
        long init(FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportSliceViewed(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        void reportNavigationStarted(long nativeFeedStreamSurface, FeedStreamSurface caller,
                String url, boolean inNewTab);
        // TODO(jianli): Call this function at the appropriate time.
        void reportPageLoaded(long nativeFeedStreamSurface, FeedStreamSurface caller, String url,
                boolean inNewTab);
        // TODO(jianli): Call this function at the appropriate time.
        void reportOpenAction(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        // TODO(jianli): Call this function at the appropriate time.
        void reportOpenInNewTabAction(
                long nativeFeedStreamSurface, FeedStreamSurface caller, String sliceId);
        // TODO(jianli): Call this function at the appropriate time.
        void reportOpenInNewIncognitoTabAction(
                long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportSendFeedbackAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportLearnMoreAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportDownloadAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportRemoveAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportNotInterestedInAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportManageInterestsAction(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportContextMenuOpened(long nativeFeedStreamSurface, FeedStreamSurface caller);
        // TODO(jianli): Call this function at the appropriate time.
        void reportStreamScrolled(
                long nativeFeedStreamSurface, FeedStreamSurface caller, int distanceDp);
        // TODO(jianli): Call this function at the appropriate time.
        void reportStreamScrollStart(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void loadMore(long nativeFeedStreamSurface, FeedStreamSurface caller);
        void processThereAndBackAgain(
                long nativeFeedStreamSurface, FeedStreamSurface caller, byte[] data);
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
