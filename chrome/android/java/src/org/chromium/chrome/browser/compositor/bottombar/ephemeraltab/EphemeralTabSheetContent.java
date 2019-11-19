// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.support.annotation.DrawableRes;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.thinwebview.ThinWebView;
import org.chromium.chrome.browser.thinwebview.ThinWebViewFactory;
import org.chromium.chrome.browser.ui.widget.FadingShadow;
import org.chromium.chrome.browser.ui.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Represents ephemeral tab content and the toolbar, which can be included inside the bottom sheet.
 */
public class EphemeralTabSheetContent implements BottomSheetContent {
    private static final float PEEK_TOOLBAR_HEIGHT_MULTIPLE = 2.f;

    private final Context mContext;
    private final Runnable mOpenNewTabCallback;
    private final Runnable mToolbarClickCallback;
    private final Runnable mCloseButtonCallback;
    private final int mToolbarHeightPx;

    private ViewGroup mToolbarView;
    private ViewGroup mSheetContentView;

    private WebContents mWebContents;
    private ContentView mWebContentView;
    private ThinWebView mThinWebView;
    private FadingShadowView mShadow;
    private Drawable mCurrentFavicon;
    private ImageView mFaviconView;

    /**
     * Constructor.
     * @param context An Android context.
     * @param openNewTabCallback Callback invoked to open a new tab.
     * @param toolbarClickCallback Callback invoked when user clicks on the toolbar.
     * @param closeButtonCallback Callback invoked when user clicks on the close button.
     * @param maxSheetHeight The height of the sheet in full height position.
     */
    public EphemeralTabSheetContent(Context context, Runnable openNewTabCallback,
            Runnable toolbarClickCallback, Runnable closeButtonCallback, int maxSheetHeight) {
        mContext = context;
        mOpenNewTabCallback = openNewTabCallback;
        mToolbarClickCallback = toolbarClickCallback;
        mCloseButtonCallback = closeButtonCallback;
        mToolbarHeightPx =
                mContext.getResources().getDimensionPixelSize(R.dimen.sheet_tab_toolbar_height);

        createThinWebView(maxSheetHeight);
        createToolbarView();
    }

    /**
     * Add web contents to the sheet.
     * @param webContents The {@link WebContents} to be displayed.
     * @param contentView The {@link ContentView} associated with the web contents.
     */
    public void attachWebContents(WebContents webContents, ContentView contentView) {
        mWebContents = webContents;
        mWebContentView = contentView;
        if (mWebContentView.getParent() != null) {
            ((ViewGroup) mWebContentView.getParent()).removeView(mWebContentView);
        }
        mThinWebView.attachWebContents(mWebContents, mWebContentView);
    }

    /**
     * Create a ThinWebView, add it to the view hierarchy, which represents the contents of the
     * bottom sheet.
     */
    private void createThinWebView(int maxSheetHeight) {
        mThinWebView = ThinWebViewFactory.create(mContext, new ActivityWindowAndroid(mContext));

        mSheetContentView = new FrameLayout(mContext);
        mThinWebView.getView().setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, maxSheetHeight - mToolbarHeightPx));
        mSheetContentView.addView(mThinWebView.getView());

        mSheetContentView.setPadding(0, mToolbarHeightPx, 0, 0);
    }

    private void createToolbarView() {
        mToolbarView =
                (ViewGroup) LayoutInflater.from(mContext).inflate(R.layout.sheet_tab_toolbar, null);
        mShadow = mToolbarView.findViewById(R.id.shadow);
        mShadow.init(ApiCompatibilityUtils.getColor(
                             mContext.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);
        ImageView openInNewTabButton = mToolbarView.findViewById(R.id.open_in_new_tab);
        openInNewTabButton.setOnClickListener(view -> mOpenNewTabCallback.run());

        View toolbar = mToolbarView.findViewById(R.id.toolbar);
        toolbar.setOnClickListener(view -> mToolbarClickCallback.run());

        View closeButton = mToolbarView.findViewById(R.id.close);
        closeButton.setOnClickListener(view -> mCloseButtonCallback.run());

        mFaviconView = mToolbarView.findViewById(R.id.favicon);
        mCurrentFavicon = mFaviconView.getDrawable();
    }

    /**
     * Resizes the thin webview as per the given sheet height. This should never be more than the
     * tab height for it to function correctly.
     * @param maxContentHeight The height of the bottom sheet in the maximized state.
     */
    void updateContentHeight(int maxContentHeight) {
        if (maxContentHeight == 0) return;
        ViewGroup.LayoutParams layoutParams = mThinWebView.getView().getLayoutParams();
        layoutParams.height = maxContentHeight - mToolbarHeightPx;
        mSheetContentView.requestLayout();
    }

    /** Method to be called to start the favicon anmiation. */
    public void startFaviconAnimation(Drawable favicon) {
        if (favicon == null) {
            mCurrentFavicon = null;
            mFaviconView.setImageDrawable(null);
            return;
        }

        // TODO(shaktisahu): Find out if there is a better way for this animation.
        Drawable presentedDrawable = favicon;
        if (mCurrentFavicon != null && !(mCurrentFavicon instanceof TransitionDrawable)) {
            TransitionDrawable transitionDrawable = ApiCompatibilityUtils.createTransitionDrawable(
                    new Drawable[] {mCurrentFavicon, favicon});
            transitionDrawable.setCrossFadeEnabled(true);
            transitionDrawable.startTransition(BottomSheetController.BASE_ANIMATION_DURATION_MS);
            presentedDrawable = transitionDrawable;
        }

        mFaviconView.setImageDrawable(presentedDrawable);
        mCurrentFavicon = favicon;
    }

    /** Sets the ephemeral tab title text. */
    public void updateTitle(String title) {
        TextView toolbarText = mToolbarView.findViewById(R.id.title);
        toolbarText.setText(title);
    }

    /** Sets the ephemeral tab URL. */
    public void updateURL(String url) {
        TextView originView = mToolbarView.findViewById(R.id.origin);
        originView.setText(UrlFormatter.formatUrlForSecurityDisplayOmitScheme(url));
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

    /** Called to show or hide the open in new tab button. */
    public void showOpenInNewTabButton(boolean show) {
        View openInNewTabButton = mToolbarView.findViewById(R.id.open_in_new_tab);
        openInNewTabButton.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    @Override
    public View getContentView() {
        return mSheetContentView;
    }

    @Nullable
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
        mThinWebView.destroy();
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        int toolbarHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        return (int) (toolbarHeight * PEEK_TOOLBAR_HEIGHT_MULTIPLE);
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.ephemeral_tab_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_closed;
    }
}
