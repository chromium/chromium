// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Canvas;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.AdapterDataObserver;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPage.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.TileGroup;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

/**
 * The native new tab page, represented by some basic data such as title and url, and an Android
 * View that displays the page.
 */
public class NewTabPageView extends FrameLayout {
    private static final String TAG = "NewTabPageView";

    private NewTabPageRecyclerView mRecyclerView;

    private NewTabPageLayout mNewTabPageLayout;

    private NewTabPageManager mManager;
    private Tab mTab;
    private SnapScrollHelper mSnapScrollHelper;
    private UiConfig mUiConfig;

    private boolean mNewTabPageRecyclerViewChanged;
    private int mSnapshotWidth;
    private int mSnapshotHeight;
    private int mSnapshotScrollY;
    private ContextMenuManager mContextMenuManager;

    /**
     * Manages the view interaction with the rest of the system.
     */
    public interface NewTabPageManager extends SuggestionsUiDelegate {
        /** @return Whether the location bar is shown in the NTP. */
        boolean isLocationBarShownInNTP();

        /** @return Whether voice search is enabled and the microphone should be shown. */
        boolean isVoiceSearchEnabled();

        /**
         * Animates the search box up into the omnibox and bring up the keyboard.
         * @param beginVoiceSearch Whether to begin a voice search.
         * @param pastedText Text to paste in the omnibox after it's been focused. May be null.
         */
        void focusSearchBox(boolean beginVoiceSearch, String pastedText);

        /**
         * @return whether the {@link NewTabPage} associated with this manager is the current page
         * displayed to the user.
         */
        boolean isCurrentPage();

        /**
         * Called when the NTP has completely finished loading (all views will be inflated
         * and any dependent resources will have been loaded).
         */
        void onLoadingComplete();
    }

    /**
     * Default constructor required for XML inflation.
     */
    public NewTabPageView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mRecyclerView = new NewTabPageRecyclerView(getContext());

        // Don't attach now, the recyclerView itself will determine when to do it.
        mNewTabPageLayout = (NewTabPageLayout) LayoutInflater.from(getContext())
                                    .inflate(R.layout.new_tab_page_layout, mRecyclerView, false);
    }

    /**
     * Initializes the NTP. This must be called immediately after inflation, before this object is
     * used in any other way.
     *
     * @param manager NewTabPageManager used to perform various actions when the user interacts
     *                with the page.
     * @param tab The Tab that is showing this new tab page.
     * @param searchProviderHasLogo Whether the search provider has a logo.
     * @param searchProviderIsGoogle Whether the search provider is Google.
     * @param scrollPosition The adapter scroll position to initialize to.
     */
    public void initialize(NewTabPageManager manager, Tab tab, TileGroup.Delegate tileGroupDelegate,
            boolean searchProviderHasLogo, boolean searchProviderIsGoogle, int scrollPosition) {
        TraceEvent.begin(TAG + ".initialize()");
        mTab = tab;
        mManager = manager;
        mUiConfig = new UiConfig(this);

        assert manager.getSuggestionsSource() != null;

        // Don't store a direct reference to the activity, because it might change later if the tab
        // is reparented.
        Runnable closeContextMenuCallback = () -> mTab.getActivity().closeContextMenu();
        mContextMenuManager = new ContextMenuManager(mManager.getNavigationDelegate(),
                mRecyclerView::setTouchEnabled, closeContextMenuCallback,
                NewTabPage.CONTEXT_MENU_USER_ACTION_PREFIX);
        mTab.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);

        mNewTabPageLayout.initialize(manager, tab, tileGroupDelegate, searchProviderHasLogo,
                searchProviderIsGoogle, mRecyclerView, mContextMenuManager, mUiConfig);

        mSnapScrollHelper = new SnapScrollHelper(mManager, mNewTabPageLayout);
        mSnapScrollHelper.setView(mRecyclerView);
        mRecyclerView.setSnapScrollHelper(mSnapScrollHelper);
        addView(mRecyclerView);

        mRecyclerView.setItemAnimator(new DefaultItemAnimator() {
            @Override
            public boolean animateMove(ViewHolder holder, int fromX, int fromY, int toX, int toY) {
                // If |mNewTabPageLayout| is animated by the RecyclerView because an item below it
                // was dismissed, avoid also manipulating its vertical offset in our scroll handling
                // at the same time. The onScrolled() method is called when an item is dismissed and
                // the item at the top of the viewport is repositioned.
                if (holder.itemView == mNewTabPageLayout) mNewTabPageLayout.setIsViewMoving(true);

                // Cancel any pending scroll update handling, a new one will be scheduled in
                // onAnimationFinished().
                mSnapScrollHelper.resetSearchBoxOnScroll(false);

                return super.animateMove(holder, fromX, fromY, toX, toY);
            }

            @Override
            public void onAnimationFinished(ViewHolder viewHolder) {
                super.onAnimationFinished(viewHolder);

                // When an item is dismissed, the items at the top of the viewport might not move,
                // and onScrolled() might not be called. We can get in the situation where the
                // toolbar buttons disappear, so schedule an update for it. This can be cancelled
                // from animateMove() in case |mNewTabPageLayout| will be moved. We don't know that
                // from here, as the RecyclerView will animate multiple items when one is dismissed,
                // and some will "finish" synchronously if they are already in the correct place,
                // before other moves have even been scheduled.
                if (viewHolder.itemView == mNewTabPageLayout) {
                    mNewTabPageLayout.setIsViewMoving(false);
                }
                mSnapScrollHelper.resetSearchBoxOnScroll(true);
            }
        });

        Profile profile = Profile.getLastUsedProfile();
        OfflinePageBridge offlinePageBridge =
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);

        initializeLayoutChangeListener();
        mNewTabPageLayout.setSearchProviderInfo(searchProviderHasLogo, searchProviderIsGoogle);

        mRecyclerView.init(mUiConfig, mContextMenuManager);

        // Set up snippets
        NewTabPageAdapter newTabPageAdapter = new NewTabPageAdapter(
                mManager, mNewTabPageLayout, mUiConfig, offlinePageBridge, mContextMenuManager);
        newTabPageAdapter.refreshSuggestions();
        mRecyclerView.setAdapter(newTabPageAdapter);
        mRecyclerView.getLinearLayoutManager().scrollToPosition(scrollPosition);

        setupScrollHandling();

        // When the NewTabPageAdapter's data changes we need to invalidate any previous
        // screen captures of the NewTabPageView.
        newTabPageAdapter.registerAdapterDataObserver(new AdapterDataObserver() {
            @Override
            public void onChanged() {
                mNewTabPageRecyclerViewChanged = true;
            }

            @Override
            public void onItemRangeChanged(int positionStart, int itemCount) {
                onChanged();
            }

            @Override
            public void onItemRangeInserted(int positionStart, int itemCount) {
                onChanged();
            }

            @Override
            public void onItemRangeRemoved(int positionStart, int itemCount) {
                onChanged();
            }

            @Override
            public void onItemRangeMoved(int fromPosition, int toPosition, int itemCount) {
                onChanged();
            }
        });

        manager.addDestructionObserver(NewTabPageView.this::onDestroy);

        TraceEvent.end(TAG + ".initialize()");
    }

    /**
     * @return The {@link NewTabPageLayout} displayed in this NewTabPageView.
     */
    NewTabPageLayout getNewTabPageLayout() {
        return mNewTabPageLayout;
    }

    /**
     * Sets the {@link FakeboxDelegate} associated with the new tab page.
     * @param fakeboxDelegate The {@link FakeboxDelegate} used to determine whether the URL bar
     *                        has focus.
     */
    public void setFakeboxDelegate(FakeboxDelegate fakeboxDelegate) {
        mRecyclerView.setFakeboxDelegate(fakeboxDelegate);
    }

    private void initializeLayoutChangeListener() {
        TraceEvent.begin(TAG + ".initializeLayoutChangeListener()");

        // Listen for layout changes on the NewTabPageView itself to catch changes in scroll
        // position that are due to layout changes after e.g. device rotation. This contrasts with
        // regular scrolling, which is observed through an OnScrollListener.
        addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight,
                                          oldBottom) -> { mSnapScrollHelper.handleScroll(); });
        TraceEvent.end(TAG + ".initializeLayoutChangeListener()");
    }

    @VisibleForTesting
    public NewTabPageRecyclerView getRecyclerView() {
        return mRecyclerView;
    }

    /**
     * Adds listeners to scrolling to take care of snap scrolling and updating the search box on
     * scroll.
     */
    private void setupScrollHandling() {
        TraceEvent.begin(TAG + ".setupScrollHandling()");
        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                mSnapScrollHelper.handleScroll();
            }
        });

        TraceEvent.end(TAG + ".setupScrollHandling()");
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        // Trigger a scroll update when reattaching the window to signal the toolbar that
        // it needs to reset the NTP state. Note that this is handled here rather than
        // NewTabPageLayout#onAttachedToWindow() because NewTabPageLayout may not be
        // immediately attached to the window if the RecyclerView is scrolled when the NTP
        // is refocused.
        if (mManager.isLocationBarShownInNTP()) mNewTabPageLayout.updateSearchBoxOnScroll();
    }

    /**
     * @see InvalidationAwareThumbnailProvider#shouldCaptureThumbnail()
     */
    boolean shouldCaptureThumbnail() {
        if (getWidth() == 0 || getHeight() == 0) return false;

        return mNewTabPageRecyclerViewChanged || mNewTabPageLayout.shouldCaptureThumbnail()
                || getWidth() != mSnapshotWidth || getHeight() != mSnapshotHeight
                || mRecyclerView.computeVerticalScrollOffset() != mSnapshotScrollY;
    }

    /**
     * @see InvalidationAwareThumbnailProvider#captureThumbnail(Canvas)
     */
    void captureThumbnail(Canvas canvas) {
        mNewTabPageLayout.onPreCaptureThumbnail();
        ViewUtils.captureBitmap(this, canvas);
        mSnapshotWidth = getWidth();
        mSnapshotHeight = getHeight();
        mSnapshotScrollY = mRecyclerView.computeVerticalScrollOffset();
        mNewTabPageRecyclerViewChanged = false;
    }

    /**
     * @return The adapter position the user has scrolled to.
     */
    public int getScrollPosition() {
        return mRecyclerView.getScrollPosition();
    }

    private void onDestroy() {
        mTab.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
    }

    @VisibleForTesting
    public SnapScrollHelper getSnapScrollHelper() {
        return mSnapScrollHelper;
    }
}
