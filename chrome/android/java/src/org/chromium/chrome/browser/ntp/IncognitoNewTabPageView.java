// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.annotation.ColorInt;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.ui.base.ViewUtils;

/** The New Tab Page for use in the incognito profile. */
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
         * Initializes the cookie controls manager for interaction with the cookie controls toggle.
         */
        void initCookieControlsManager();

        /** Tells the caller whether a new snapshot is required or not. */
        boolean shouldCaptureThumbnail();

        /** Whether to show the tracking protection UI on the NTP. */
        boolean shouldShowTrackingProtectionNtp();

        /** Cleans up the manager after it is finished being used. */
        void destroy();

        /**
         * Called when the NTP has completely finished loading (all views will be inflated
         * and any dependent resources will have been loaded).
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

        // Inflate the correct cookie/tracking protection card.
        ViewStub cardStub = findViewById(R.id.cookie_card_stub);
        if (cardStub == null) return;
        if (mManager.shouldShowTrackingProtectionNtp()) {
            cardStub.setLayoutResource(R.layout.incognito_tracking_protection_card);
        } else {
            cardStub.setLayoutResource(R.layout.incognito_cookie_controls_card);
        }
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

    /**
     * Initialize the incognito New Tab Page.
     * @param manager The manager that handles external dependencies of the view.
     */
    void initialize(IncognitoNewTabPageManager manager) {
        mManager = manager;
        inflateConditionalLayouts();
        mManager.initCookieControlsManager();
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

        return mManager.shouldCaptureThumbnail()
                || getWidth() != mSnapshotWidth
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

    /**
     * Set the toggle on the cookie controls card.
     * @param isChecked Whether it's checked or not.
     */
    void setIncognitoCookieControlsToggleChecked(boolean isChecked) {
        mDescriptionView.setCookieControlsToggle(isChecked);
    }

    /**
     * Set the incognito cookie controls toggle checked change listener.
     * @param listener The given checked change listener.
     */
    void setIncognitoCookieControlsToggleCheckedListener(OnCheckedChangeListener listener) {
        mDescriptionView.setCookieControlsToggleOnCheckedChangeListener(listener);
    }

    /**
     * Set the enforcement rule for the incognito cookie controls toggle.
     * @param enforcement The enforcement enum to set.
     */
    void setIncognitoCookieControlsToggleEnforcement(@CookieControlsEnforcement int enforcement) {
        mDescriptionView.setCookieControlsEnforcement(enforcement);
    }

    /**
     * Set the incognito cookie controls icon click listener.
     * @param listener The given onclick listener.
     */
    void setIncognitoCookieControlsIconOnclickListener(OnClickListener listener) {
        mDescriptionView.setCookieControlsIconOnclickListener(listener);
    }
}
