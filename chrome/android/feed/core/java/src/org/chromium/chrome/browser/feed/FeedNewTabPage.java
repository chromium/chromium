// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.client.stream.Stream;
import com.google.android.libraries.feed.api.host.action.ActionApi;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.feed.action.FeedActionHandler;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage
        extends NewTabPage implements FeedSurfaceCoordinator.FeedSurfaceDelegate {
    private final ContextMenuManager mContextMenuManager;
    private FeedSurfaceCoordinator mCoordinator;

    /**
     * Constructs a new {@link FeedNewTabPage}.
     *
     * @param activity The containing {@link ChromeActivity}.
     * @param nativePageHost The host for this native page.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param activityTabProvider Allows us to check if we are the current tab.
     * @param activityLifecycleDispatcher Allows us to subscribe to backgrounding events.
     */
    public FeedNewTabPage(ChromeActivity activity, NativePageHost nativePageHost,
            TabModelSelector tabModelSelector, ActivityTabProvider activityTabProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        super(activity, nativePageHost, tabModelSelector, activityTabProvider,
                activityLifecycleDispatcher);

        // Don't store a direct reference to the activity, because it might change later if the tab
        // is reparented.
        // TODO(twellington): Move this somewhere it can be shared with NewTabPageView?
        Runnable closeContextMenuCallback = () -> activity.closeContextMenu();
        mContextMenuManager = new ContextMenuManager(mNewTabPageManager.getNavigationDelegate(),
                mCoordinator.getTouchEnabledDelegate(), closeContextMenuCallback,
                NewTabPage.CONTEXT_MENU_USER_ACTION_PREFIX);
        mTab.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);

        OverviewModeBehavior overviewModeBehavior = activity instanceof ChromeTabbedActivity
                ? activity.getOverviewModeBehavior()
                : null;

        mNewTabPageLayout.initialize(mNewTabPageManager, activity, overviewModeBehavior,
                mTileGroupDelegate, mSearchProviderHasLogo,
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                mCoordinator.getScrollDelegate(), mContextMenuManager, mCoordinator.getUiConfig());
    }

    @Override
    protected void initializeMainView(Context context, NativePageHost host) {
        ActionApi actionApi = new FeedActionHandler(mNewTabPageManager.getNavigationDelegate(),
                FeedProcessScopeFactory.getFeedConsumptionObserver(),
                FeedProcessScopeFactory.getFeedOfflineIndicator(),
                OfflinePageBridge.getForProfile(mTab.getProfile()),
                FeedProcessScopeFactory.getFeedLoggingBridge());
        LayoutInflater inflater = LayoutInflater.from(mTab.getActivity());
        mNewTabPageLayout = (NewTabPageLayout) inflater.inflate(R.layout.new_tab_page_layout, null);
        SectionHeaderView sectionHeaderView = (SectionHeaderView) inflater.inflate(
                R.layout.new_tab_page_snippets_expandable_header, null, false);
        mCoordinator = new FeedSurfaceCoordinator(mTab.getActivity(),
                host.createHistoryNavigationDelegate(),
                new SnapScrollHelper(mNewTabPageManager, mNewTabPageLayout), mNewTabPageLayout,
                sectionHeaderView, actionApi,
                mTab.getActivity().getNightModeStateProvider().isInNightMode(), this);

        // Record the timestamp at which the new tab page's construction started.
        NewTabPageUma.trackTimeToFirstDraw(mCoordinator.getView(), mConstructedTimeNs);
    }

    @Override
    public void destroy() {
        super.destroy();
        mCoordinator.destroy();
        mTab.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
    }

    @Override
    public View getView() {
        return mCoordinator.getView();
    }

    @Override
    protected void saveLastScrollPosition() {
        // This behavior is handled by the StreamLifecycleManager and the Feed library.
    }

    @Override
    public boolean shouldCaptureThumbnail() {
        return mNewTabPageLayout.shouldCaptureThumbnail() || mCoordinator.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mNewTabPageLayout.onPreCaptureThumbnail();
        mCoordinator.captureThumbnail(canvas);
    }

    // Implements FeedSurfaceDelegate
    @Override
    public StreamLifecycleManager createStreamLifecycleManager(Stream stream, Activity activity) {
        return new NtpStreamLifecycleManager(stream, activity, mTab);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return !(mTab != null && DeviceFormFactor.isWindowOnTablet(mTab.getWindowAndroid()))
                && (mFakeboxDelegate != null && mFakeboxDelegate.isUrlBarFocused());
    }

    @VisibleForTesting
    public static boolean isDummy() {
        return false;
    }

    @VisibleForTesting
    FeedSurfaceCoordinator getCoordinatorForTesting() {
        return mCoordinator;
    }

    @VisibleForTesting
    FeedSurfaceMediator getMediatorForTesting() {
        return mCoordinator.getMediatorForTesting();
    }

    @VisibleForTesting
    public Stream getStreamForTesting() {
        return mCoordinator.getStream();
    }

    @Override
    public View getSignInPromoViewForTesting() {
        return mCoordinator.getSigninPromoView();
    }

    @Override
    public View getSectionHeaderViewForTesting() {
        return mCoordinator.getSectionHeaderView();
    }
}
