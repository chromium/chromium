// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.res.Resources;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.ScrollView;

import com.google.android.libraries.feed.api.stream.ContentChangedListener;
import com.google.android.libraries.feed.api.stream.ScrollListener;
import com.google.android.libraries.feed.api.stream.Stream;

import org.chromium.base.MemoryPressureListener;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.SigninPromoUtil;

/**
 * A mediator for the {@link FeedNewTabPage} responsible for interacting with the
 * native library and handling business logic.
 */
class FeedNewTabPageMediator
        implements NewTabPageLayout.ScrollDelegate, ContextMenuManager.TouchEnabledDelegate {
    private final FeedNewTabPage mCoordinator;
    private final SnapScrollHelper mSnapScrollHelper;
    private final PrefChangeRegistrar mPrefChangeRegistrar;

    private ScrollListener mStreamScrollListener;
    private ContentChangedListener mStreamContentChangedListener;
    private SectionHeader mSectionHeader;
    private MemoryPressureCallback mMemoryPressureCallback;
    private @Nullable SignInPromo mSignInPromo;

    private boolean mFeedEnabled;
    private boolean mTouchEnabled = true;
    private boolean mStreamContentChanged;
    private int mThumbnailWidth;
    private int mThumbnailHeight;
    private int mThumbnailScrollY;

    /**
     * @param feedNewTabPage The {@link FeedNewTabPage} that interacts with this class.
     * @param snapScrollHelper The {@link SnapScrollHelper} that handles snap scrolling.
     */
    FeedNewTabPageMediator(FeedNewTabPage feedNewTabPage, SnapScrollHelper snapScrollHelper) {
        mCoordinator = feedNewTabPage;
        mSnapScrollHelper = snapScrollHelper;

        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mPrefChangeRegistrar.addObserver(Pref.NTP_ARTICLES_SECTION_ENABLED, this::updateContent);
        initialize();
        // Create the content.
        updateContent();
    }

    /** Clears any dependencies. */
    void destroy() {
        destroyPropertiesForStream();
        mPrefChangeRegistrar.destroy();
    }

    private void initialize() {
        // Listen for layout changes on the NewTabPageView itself to catch changes in scroll
        // position that are due to layout changes after e.g. device rotation. This contrasts with
        // regular scrolling, which is observed through an OnScrollListener.
        mCoordinator.getView().addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    mSnapScrollHelper.handleScroll();
                });
    }

    /** Update the content based on supervised user or enterprise policy. */
    private void updateContent() {
        mFeedEnabled = FeedProcessScopeFactory.isFeedProcessEnabled();
        if ((mFeedEnabled && mCoordinator.getStream() != null)
                || (!mFeedEnabled && mCoordinator.getScrollViewForPolicy() != null))
            return;

        if (mFeedEnabled) {
            mCoordinator.createStream();
            mSnapScrollHelper.setView(mCoordinator.getStream().getView());
            initializePropertiesForStream();
        } else {
            destroyPropertiesForStream();
            mCoordinator.createScrollViewForPolicy();
            mSnapScrollHelper.setView(mCoordinator.getScrollViewForPolicy());
            initializePropertiesForPolicy();
        }
    }

    /**
     * Initialize properties for UI components in the {@link FeedNewTabPage}.
     * TODO(huayinz): Introduce a Model for these properties.
     */
    private void initializePropertiesForStream() {
        Stream stream = mCoordinator.getStream();
        mStreamScrollListener = new ScrollListener() {
            @Override
            public void onScrollStateChanged(int state) {}

            @Override
            public void onScrolled(int dx, int dy) {
                mSnapScrollHelper.handleScroll();
            }
        };
        stream.addScrollListener(mStreamScrollListener);

        mStreamContentChangedListener = () -> {
            mStreamContentChanged = true;
            mSnapScrollHelper.resetSearchBoxOnScroll(true);
        };
        stream.addOnContentChangedListener(mStreamContentChangedListener);

        boolean suggestionsVisible =
                PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE);
        Resources res = mCoordinator.getSectionHeaderView().getResources();
        mSectionHeader =
                new SectionHeader(res.getString(R.string.ntp_article_suggestions_section_header),
                        suggestionsVisible, this::onSectionHeaderToggled);
        mPrefChangeRegistrar.addObserver(Pref.NTP_ARTICLES_LIST_VISIBLE, this::updateSectionHeader);
        mCoordinator.getSectionHeaderView().setHeader(mSectionHeader);
        stream.setStreamContentVisibility(mSectionHeader.isExpanded());

        if (SignInPromo.shouldCreatePromo()) {
            mSignInPromo = new FeedSignInPromo();
            mSignInPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }

        mCoordinator.updateHeaderViews(mSignInPromo != null && mSignInPromo.isVisible());

        mMemoryPressureCallback = pressure -> mCoordinator.getStream().trim();
        MemoryPressureListener.addCallback(mMemoryPressureCallback);
    }

    /** Clear any dependencies related to the {@link Stream}. */
    private void destroyPropertiesForStream() {
        Stream stream = mCoordinator.getStream();
        if (stream == null) return;

        stream.removeScrollListener(mStreamScrollListener);
        stream.removeOnContentChangedListener(mStreamContentChangedListener);
        MemoryPressureListener.removeCallback(mMemoryPressureCallback);
        if (mSignInPromo != null) mSignInPromo.destroy();
        mPrefChangeRegistrar.removeObserver(Pref.NTP_ARTICLES_LIST_VISIBLE);
        mStreamScrollListener = null;
        mStreamContentChangedListener = null;
        mMemoryPressureCallback = null;
        mSignInPromo = null;
    }

    /**
     * Initialize properties for the scroll view shown under supervised user or enterprise policy.
     */
    private void initializePropertiesForPolicy() {
        ScrollView view = mCoordinator.getScrollViewForPolicy();
        view.getViewTreeObserver().addOnScrollChangedListener(mSnapScrollHelper::handleScroll);
    }

    /** Update whether the section header should be expanded. */
    private void updateSectionHeader() {
        boolean suggestionsVisible =
                PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE);
        if (mSectionHeader.isExpanded() != suggestionsVisible) mSectionHeader.toggleHeader();
        if (mSignInPromo != null) {
            mSignInPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }
        mStreamContentChanged = true;
    }

    /**
     * Callback on section header toggled. This will update the visibility of the Feed and the
     * expand icon on the section header view.
     */
    private void onSectionHeaderToggled() {
        PrefServiceBridge.getInstance().setBoolean(
                Pref.NTP_ARTICLES_LIST_VISIBLE, mSectionHeader.isExpanded());
        mCoordinator.getStream().setStreamContentVisibility(mSectionHeader.isExpanded());
        // TODO(huayinz): Update the section header view through a ModelChangeProcessor.
        mCoordinator.getSectionHeaderView().updateVisuals();
    }

    /**
     * Callback on sign-in promo is dismissed.
     */
    void onSignInPromoDismissed() {
        View view = mCoordinator.getSigninPromoView();
        mSignInPromo.dismiss(removedItemTitle
                -> view.announceForAccessibility(view.getResources().getString(
                        R.string.ntp_accessibility_item_removed, removedItemTitle)));
    }

    /** Whether a new thumbnail should be captured. */
    boolean shouldCaptureThumbnail() {
        return mStreamContentChanged || mCoordinator.getView().getWidth() != mThumbnailWidth
                || mCoordinator.getView().getHeight() != mThumbnailHeight
                || getVerticalScrollOffset() != mThumbnailScrollY;
    }

    /** Reset all the properties for thumbnail capturing after a new thumbnail is captured. */
    void onThumbnailCaptured() {
        mThumbnailWidth = mCoordinator.getView().getWidth();
        mThumbnailHeight = mCoordinator.getView().getHeight();
        mThumbnailScrollY = getVerticalScrollOffset();
        mStreamContentChanged = false;
    }

    /**
     * @return Whether the touch events are enabled on the {@link FeedNewTabPage}.
     * TODO(huayinz): Move this method to a Model once a Model is introduced.
     */
    boolean getTouchEnabled() {
        return mTouchEnabled;
    }

    // TouchEnabledDelegate interface.

    @Override
    public void setTouchEnabled(boolean enabled) {
        mTouchEnabled = enabled;
    }

    // ScrollDelegate interface.

    @Override
    public boolean isScrollViewInitialized() {
        if (mFeedEnabled) {
            Stream stream = mCoordinator.getStream();
            // During startup the view may not be fully initialized, so we check to see if some
            // basic view properties (height of the RecyclerView) are sane.
            return stream != null && stream.getView().getHeight() > 0;
        } else {
            ScrollView scrollView = mCoordinator.getScrollViewForPolicy();
            return scrollView != null && scrollView.getHeight() > 0;
        }
    }

    @Override
    public int getVerticalScrollOffset() {
        // This method returns a valid vertical scroll offset only when the first header view in the
        // Stream is visible.
        if (!isScrollViewInitialized()) return 0;

        if (mFeedEnabled) {
            int firstChildTop = mCoordinator.getStream().getChildTopAt(0);
            return firstChildTop != Stream.POSITION_NOT_KNOWN ? -firstChildTop : Integer.MIN_VALUE;
        } else {
            return mCoordinator.getScrollViewForPolicy().getScrollY();
        }
    }

    @Override
    public boolean isChildVisibleAtPosition(int position) {
        if (!isScrollViewInitialized()) return false;

        if (mFeedEnabled) {
            return mCoordinator.getStream().isChildAtPositionVisible(position);
        } else {
            ScrollView scrollView = mCoordinator.getScrollViewForPolicy();
            Rect rect = new Rect();
            scrollView.getHitRect(rect);
            return scrollView.getChildAt(position).getLocalVisibleRect(rect);
        }
    }

    @Override
    public void snapScroll() {
        if (!isScrollViewInitialized()) return;

        int initialScroll = getVerticalScrollOffset();
        int scrollTo = mSnapScrollHelper.calculateSnapPosition(initialScroll);

        // Calculating the snap position should be idempotent.
        assert scrollTo == mSnapScrollHelper.calculateSnapPosition(scrollTo);

        if (mFeedEnabled) {
            mCoordinator.getStream().smoothScrollBy(0, scrollTo - initialScroll);
        } else {
            mCoordinator.getScrollViewForPolicy().smoothScrollBy(0, scrollTo - initialScroll);
        }
    }

    /**
     * The {@link SignInPromo} for the Feed.
     * TODO(huayinz): Update content and visibility through a ModelChangeProcessor.
     */
    private class FeedSignInPromo extends SignInPromo {
        FeedSignInPromo() {
            updateSignInPromo();
        }

        @Override
        protected void setVisibilityInternal(boolean visible) {
            if (isVisible() == visible) return;

            super.setVisibilityInternal(visible);
            mCoordinator.updateHeaderViews(visible);
        }

        @Override
        protected void notifyDataChanged() {
            updateSignInPromo();
        }

        /** Update the content displayed in {@link PersonalizedSigninPromoView}. */
        private void updateSignInPromo() {
            SigninPromoUtil.setupPromoViewFromCache(mSigninPromoController, mProfileDataCache,
                    mCoordinator.getSigninPromoView(), null);
        }
    }

    // TODO(huayinz): Return the Model for testing in Coordinator instead once a Model is created.
    @VisibleForTesting
    SectionHeader getSectionHeaderForTesting() {
        return mSectionHeader;
    }

    @VisibleForTesting
    SignInPromo getSignInPromoForTesting() {
        return mSignInPromo;
    }
}
