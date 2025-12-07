// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.ui.base.ViewUtils;

/** The New Tab Page for use in the incognito profile. */
@NullMarked
public class IncognitoNewTabPageView extends FrameLayout {
    private IncognitoNewTabPageManager mManager;
    private boolean mFirstShow = true;
    private FadingShadowView mFadingShadowBottom;
    private NewTabPageScrollView mScrollView;
    private IncognitoDescriptionView mDescriptionView;

    private int mSnapshotWidth;
    private int mSnapshotHeight;
    private int mSnapshotScrollY;

    /** Manages the view interaction with the rest of the system. */
    interface IncognitoNewTabPageManager {
        /** Loads a page explaining details about incognito mode in the current tab. */
        void loadIncognitoLearnMore();

        /**
         * Called when the NTP has completely finished loading (all views will be inflated and any
         * dependent resources will have been loaded).
         */
        void onLoadingComplete();
    }

    /** Default constructor needed to inflate via XML. */
    public IncognitoNewTabPageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        @ColorInt int bgColor = getContext().getColor(R.color.ntp_bg_incognito);
        mScrollView = findViewById(R.id.ntp_scrollview);
        mScrollView.setBackgroundColor(bgColor);
        setContentDescription(
                getResources().getText(R.string.accessibility_new_incognito_tab_page));

        // FOCUS_BEFORE_DESCENDANTS is needed to support keyboard shortcuts. Otherwise, pressing
        // any shortcut causes the UrlBar to be focused. See ViewRootImpl.leaveTouchMode().
        mScrollView.setDescendantFocusability(FOCUS_BEFORE_DESCENDANTS);
        mFadingShadowBottom = findViewById(R.id.shadow_bottom);
        mFadingShadowBottom.init(bgColor, FadingShadow.POSITION_BOTTOM);
        mScrollView.setOnScrollChangeListener(
                new OnScrollChangeListener() {
                    @Override
                    public void onScrollChange(
                            View v, int scrollX, int scrollY, int oldScrollX, int oldScrollY) {
                        mFadingShadowBottom.setVisibility(
                                mScrollView.canScrollVertically(1) ? View.VISIBLE : View.GONE);
                    }
                });
    }

    /**
     * Initialize the incognito New Tab Page.
     *
     * @param manager The manager that handles external dependencies of the view.
     */
    @Initializer
    void initialize(IncognitoNewTabPageManager manager) {
        mManager = manager;
        inflateConditionalLayouts();
    }

    private void inflateConditionalLayouts() {
        ViewStub viewStub = findViewById(R.id.incognito_description_layout_stub);
        viewStub.setLayoutResource(R.layout.incognito_description_layout);
        mDescriptionView = (IncognitoDescriptionView) viewStub.inflate();
        mDescriptionView.setLearnMoreOnclickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        mManager.loadIncognitoLearnMore();
                    }
                });

        // Inflate the tracking protection card.
        ViewStub cardStub = findViewById(R.id.cookie_card_stub);
        if (cardStub == null) return;
        cardStub.setLayoutResource(R.layout.incognito_tracking_protection_card);
        cardStub.inflate();
        mDescriptionView.formatTrackingProtectionText(getContext(), this);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        assert mManager != null;
        if (mFirstShow) {
            mManager.onLoadingComplete();
            mFirstShow = false;
        }
    }

    /** @return The IncognitoNewTabPageManager associated with this IncognitoNewTabPageView. */
    protected IncognitoNewTabPageManager getManager() {
        return mManager;
    }

    /**
     * @return The ScrollView of within the page. Used for padding when drawing edge to edge.
     */
    ScrollView getScrollView() {
        return mScrollView;
    }

    /**
     * @see org.chromium.chrome.browser.compositor.layouts.content.
     *     InvalidationAwareThumbnailProvider#shouldCaptureThumbnail()
     */
    boolean shouldCaptureThumbnail() {
        if (getWidth() == 0 || getHeight() == 0) return false;

        return getWidth() != mSnapshotWidth
                || getHeight() != mSnapshotHeight
                || mScrollView.getScrollY() != mSnapshotScrollY;
    }

    /**
     * @see org.chromium.chrome.browser.compositor.layouts.content.
     *         InvalidationAwareThumbnailProvider#captureThumbnail(Canvas)
     */
    void captureThumbnail(Canvas canvas) {
        ViewUtils.captureBitmap(this, canvas);
        mSnapshotWidth = getWidth();
        mSnapshotHeight = getHeight();
        mSnapshotScrollY = mScrollView.getScrollY();
    }
}
