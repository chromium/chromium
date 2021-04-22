// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * An implementation of {@link BottomSheetContent} for the merchant trust details page experience.
 */
public class MerchantTrustDetailsSheetContent implements BottomSheetContent {
    private final Context mContext;
    private final int mToolbarHeightPx;
    private final Runnable mCloseButtonCallback;

    private ViewGroup mToolbarView;
    private FadingShadowView mShadow;
    private WebContents mWebContents;
    private ContentView mWebContentView;
    private ThinWebView mThinWebView;
    private ViewGroup mSheetContentView;
    private ImageView mFaviconView;

    /** Ratio of the height when in half mode. */
    private static final float HALF_HEIGHT_RATIO = 0.6f;

    /** Ratio of the height when in full mode. Used in half-open variation. */
    private static final float FULL_HEIGHT_RATIO = 0.9f;

    /**
     * Creates a new instance.
     * @param context context instance.
     * @param maxViewHeight a provider to calculate the maximum height for the view.
     */
    public MerchantTrustDetailsSheetContent(
            Context context, Runnable closeButtonCallback, Supplier<Integer> maxViewHeight) {
        mContext = context;
        mCloseButtonCallback = closeButtonCallback;
        mToolbarHeightPx =
                mContext.getResources().getDimensionPixelSize(R.dimen.sheet_tab_toolbar_height);
        createThinWebView((int) (maxViewHeight.get() * FULL_HEIGHT_RATIO));
        createToolbarView();
    }

    /**
     * Add web contents to the sheet.
     * @param webContents The {@link WebContents} to be displayed.
     * @param contentView The {@link ContentView} associated with the web contents.
     * @param delegate The {@link WebContentsDelegateAndroid} that handles requests on WebContents.
     */
    public void attachWebContents(
            WebContents webContents, ContentView contentView, WebContentsDelegateAndroid delegate) {
        mWebContents = webContents;
        mWebContentView = contentView;
        if (mWebContentView.getParent() != null) {
            ((ViewGroup) mWebContentView.getParent()).removeView(mWebContentView);
        }
        mThinWebView.attachWebContents(mWebContents, mWebContentView, delegate);
    }

    /** Sets the title of the bottom sheet. */
    public void setTitle(String title) {
        TextView toolbarText = mToolbarView.findViewById(R.id.title);
        toolbarText.setText(title);
    }

    /** Sets the second line in the toolbar to the the provided URL. */
    public void setUrl(GURL url) {
        TextView originView = mToolbarView.findViewById(R.id.origin);
        originView.setText(
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS));
    }

    /** Sets the security icon. */
    public void setSecurityIcon(@DrawableRes int resId) {
        ImageView securityIcon = mToolbarView.findViewById(R.id.security_icon);
        securityIcon.setImageResource(resId);
    }

    /** Sets the progress on the progress bar. */
    public void setProgress(float progress) {
        ProgressBar progressBar = mToolbarView.findViewById(R.id.progress_bar);
        progressBar.setProgress(Math.round(progress * 100));
    }

    /** Called to show or hide the progress bar. */
    public void setProgressVisible(boolean visible) {
        ProgressBar progressBar = mToolbarView.findViewById(R.id.progress_bar);
        progressBar.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /**
     * Resizes the thin webview as per the given new max height.
     * @param maxViewHeight The maximum height of the view.
     */
    void updateContentHeight(int maxViewHeight) {
        if (maxViewHeight == 0) return;
        ViewGroup.LayoutParams layoutParams = mThinWebView.getView().getLayoutParams();

        // This should never be more than the tab height for it to function correctly.
        // We scale it by |FULL_HEIGHT_RATIO| to make the size equal to that of
        // ThinWebView and so it can leave a portion of the page below it visible.
        layoutParams.height = (int) (maxViewHeight * FULL_HEIGHT_RATIO) - mToolbarHeightPx;
        mSheetContentView.requestLayout();
    }

    private void createThinWebView(int maxSheetHeight) {
        mThinWebView = ThinWebViewFactory.create(mContext, new ThinWebViewConstraints());
        mSheetContentView = new FrameLayout(mContext);
        mThinWebView.getView().setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, maxSheetHeight - mToolbarHeightPx));
        mSheetContentView.addView(mThinWebView.getView());

        mSheetContentView.setPadding(0, mToolbarHeightPx, 0, 0);
    }

    @Override
    public View getContentView() {
        return mSheetContentView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mWebContents == null
                ? 0
                : RenderCoordinates.fromWebContents(mWebContents).getScrollYPixInt();
    }

    @Override
    public void destroy() {
        if (mThinWebView != null) {
            mThinWebView.destroy();
        }
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HALF_HEIGHT_RATIO;
    }

    @Override
    public float getFullHeightRatio() {
        return FULL_HEIGHT_RATIO;
    }

    @Override
    public boolean handleBackPress() {
        mCloseButtonCallback.run();
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.merchant_viewer_preview_sheet_title;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.merchant_viewer_preview_sheet_title;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.merchant_viewer_preview_sheet_title;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.merchant_viewer_preview_sheet_title;
    }

    private void createToolbarView() {
        mToolbarView =
                (ViewGroup) LayoutInflater.from(mContext).inflate(R.layout.sheet_tab_toolbar, null);
        mShadow = mToolbarView.findViewById(R.id.shadow);
        mShadow.init(ApiCompatibilityUtils.getColor(
                             mContext.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);

        View closeButton = mToolbarView.findViewById(R.id.close);
        closeButton.setOnClickListener(view -> mCloseButtonCallback.run());

        mFaviconView = mToolbarView.findViewById(R.id.favicon);
        mFaviconView.setImageResource(R.drawable.ic_logo_googleg_24dp);
    }
}