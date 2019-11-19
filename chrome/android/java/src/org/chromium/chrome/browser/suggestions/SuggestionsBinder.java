// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.media.ThumbnailUtils;
import android.support.v4.text.BidiFormatter;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

/**
 * This class is directly connected to suggestions view holders. It takes over the responsibility
 * of the view holder to update information on the views on the suggestion card.
 */
public class SuggestionsBinder {
    private static final String ARTICLE_AGE_FORMAT_STRING = " - %s";
    private static final int FADE_IN_ANIMATION_TIME_MS = 300;
    private static final int MAX_HEADER_LINES = 3;
    private static final int MAX_HEADER_LINES_WITH_SNIPPET = 2;
    private static final int MAX_SNIPPET_LINES = 3;

    private final ImageFetcher mImageFetcher;
    private final SuggestionsUiDelegate mUiDelegate;

    protected final View mCardContainerView;
    private final LinearLayout mTextLayout;
    private final TextView mHeadlineTextView;
    private final TextView mPublisherTextView;
    protected final TextView mAgeTextView;
    // TODO(twellington): Try to change this back to a TintedImageView. This was changed for a crash
    // in contextual suggestions that occurs when trying to mutate state when tinting a
    // LayerDrawable that contains a RoundedBitmapDrawable on older versions of Android.
    private final ImageView mThumbnailView;
    private final @Nullable ImageView mVideoBadge;
    private final @Nullable ImageView mOfflineBadge;
    protected final @Nullable View mPublisherBar;
    private final int mThumbnailSize;

    boolean mShowThumbnail;
    boolean mHasVideoBadge;
    boolean mHasOfflineBadge;

    private SnippetArticle mSuggestion;

    /**
     * Creates a new SuggestionsBinder.
     * @param cardContainerView The root container view for the card.
     * @param uiDelegate The interface between the suggestion surface and the rest of the browser.
     */
    public SuggestionsBinder(View cardContainerView, SuggestionsUiDelegate uiDelegate) {
        mCardContainerView = cardContainerView;
        mUiDelegate = uiDelegate;
        mImageFetcher = uiDelegate.getImageFetcher();

        mTextLayout = mCardContainerView.findViewById(R.id.text_layout);
        mThumbnailView = mCardContainerView.findViewById(R.id.article_thumbnail);
        mHeadlineTextView = mCardContainerView.findViewById(R.id.article_headline);
        mPublisherTextView = mCardContainerView.findViewById(R.id.article_publisher);
        mAgeTextView = mCardContainerView.findViewById(R.id.article_age);
        mVideoBadge = mCardContainerView.findViewById(R.id.video_badge);
        mOfflineBadge = mCardContainerView.findViewById(R.id.offline_icon);
        mPublisherBar = mCardContainerView.findViewById(R.id.publisher_bar);

        mThumbnailSize = getThumbnailSize();
    }

    public void updateViewInformation(SnippetArticle suggestion) {
        mSuggestion = suggestion;

        mHeadlineTextView.setText(suggestion.mTitle);
        if (mPublisherTextView != null) {
            mPublisherTextView.setText(getPublisherString(suggestion));
        }
        mAgeTextView.setText(getArticleAge(suggestion));

        setFavicon();
        setThumbnail();
    }

    public void updateFieldsVisibility(boolean showHeadline, boolean showThumbnail,
            boolean showThumbnailVideoBadge, boolean showSnippet) {
        mHeadlineTextView.setVisibility(showHeadline ? View.VISIBLE : View.GONE);
        mHeadlineTextView.setMaxLines(
                showSnippet ? MAX_HEADER_LINES_WITH_SNIPPET : MAX_HEADER_LINES);
        mThumbnailView.setVisibility(showThumbnail ? View.VISIBLE : View.GONE);
        mHasVideoBadge = showThumbnailVideoBadge;
        mShowThumbnail = showThumbnail;
        updateVisibilityForBadges();

        mTextLayout.setMinimumHeight(showThumbnail ? mThumbnailSize : 0);

        if (mPublisherBar != null) {
            ViewGroup.MarginLayoutParams publisherBarParams =
                    (ViewGroup.MarginLayoutParams) mPublisherBar.getLayoutParams();
            if (showHeadline) {
                // When we show a headline and not a description, we reduce the top margin of the
                // publisher bar.
                publisherBarParams.topMargin = mPublisherBar.getResources().getDimensionPixelSize(
                        R.dimen.snippets_publisher_margin_top);
            } else {
                // When there is no headline and no description, we remove the top margin of the
                // publisher bar.
                publisherBarParams.topMargin = 0;
            }
            mPublisherBar.setLayoutParams(publisherBarParams);
        }
    }

    public void updateOfflineBadgeVisibility(boolean visible) {
        mHasOfflineBadge = visible;
        updateVisibilityForBadges();
    }

    private void updateVisibilityForBadges() {
        // Never show both the video and offline badges. That would be overpromising as the video is
        // not available offline, just the page that embeds it. It would also clutter the UI. The
        // offline badge takes precedence.
        if (mVideoBadge != null) {
            mVideoBadge.setVisibility(
                    mHasVideoBadge && !mHasOfflineBadge ? View.VISIBLE : View.GONE);
        }

        if (mOfflineBadge != null) {
            mOfflineBadge.setVisibility(mHasOfflineBadge ? View.VISIBLE : View.GONE);
        }
    }

    private void setFavicon() {
        int publisherFaviconSizePx = getPublisherFaviconSizePx();
        Drawable favicon = mSuggestion.getPublisherFavicon();
        if (favicon != null) {
            setFaviconOnView(favicon, publisherFaviconSizePx);
            return;
        }

        // Set the favicon of the publisher.
        // We start initialising with the default favicon to reserve the space and prevent the text
        // from moving later.
        setDefaultFaviconOnView(publisherFaviconSizePx);
        Callback<Bitmap> faviconCallback = bitmap -> {
            Drawable drawable = new BitmapDrawable(
                    getPublisherIconTextView().getContext().getResources(), bitmap);
            // If the device has sufficient memory, store the favicon to skip the download task
            // next time we display this snippet.
            if (!SysUtils.isLowEndDevice() && mSuggestion != null) {
                mSuggestion.setPublisherFavicon(mUiDelegate.getReferencePool().put(drawable));
            }
            setFaviconOnView(drawable, publisherFaviconSizePx);
        };

        mImageFetcher.makeFaviconRequest(mSuggestion, faviconCallback);
    }

    private void setThumbnail() {
        // mThumbnailView's visibility is modified in updateFieldsVisibility().
        if (mThumbnailView.getVisibility() != View.VISIBLE) return;

        Drawable thumbnail = mSuggestion.getThumbnail();
        if (thumbnail != null) {
            setThumbnail(thumbnail);
            return;
        }

        // Temporarily set placeholder and then fetch the thumbnail from a provider.
        mThumbnailView.setBackground(null);
        ColorDrawable colorDrawable =
                new ColorDrawable(mSuggestion.getThumbnailDominantColor() != null
                                ? mSuggestion.getThumbnailDominantColor()
                                : ApiCompatibilityUtils.getColor(mThumbnailView.getResources(),
                                        R.color.thumbnail_placeholder_on_primary_bg));

        mThumbnailView.setImageDrawable(colorDrawable);

        // Fetch thumbnail for the current article.
        mImageFetcher.makeArticleThumbnailRequest(
                mSuggestion, new FetchThumbnailCallback(mSuggestion, mThumbnailSize));
    }

    private void setThumbnail(Drawable thumbnail) {
        assert thumbnail != null;

        mThumbnailView.setScaleType(ImageView.ScaleType.CENTER_CROP);
        mThumbnailView.setBackground(null);
        mThumbnailView.setImageDrawable(thumbnail);
    }

    private void setDefaultFaviconOnView(int faviconSizePx) {
        setFaviconOnView(ApiCompatibilityUtils.getDrawable(
                                 getPublisherIconTextView().getContext().getResources(),
                                 R.drawable.default_favicon),
                faviconSizePx);
    }

    private void setFaviconOnView(Drawable drawable, int faviconSizePx) {
        drawable.setBounds(0, 0, faviconSizePx, faviconSizePx);
        getPublisherIconTextView().setCompoundDrawablesRelative(drawable, null, null, null);
        getPublisherIconTextView().setVisibility(View.VISIBLE);
    }

    private void fadeThumbnailIn(Drawable thumbnail) {
        assert mThumbnailView.getDrawable() != null;

        mThumbnailView.setScaleType(ImageView.ScaleType.CENTER_CROP);
        mThumbnailView.setBackground(null);
        int duration = FADE_IN_ANIMATION_TIME_MS;
        if (CompositorAnimationHandler.isInTestingMode()) {
            mThumbnailView.setImageDrawable(thumbnail);
            return;
        }

        // Cross-fade between the placeholder and the thumbnail. We cross-fade because the incoming
        // image may have transparency and we don't want the previous image showing up behind.
        Drawable[] layers = {mThumbnailView.getDrawable(), thumbnail};
        TransitionDrawable transitionDrawable =
                ApiCompatibilityUtils.createTransitionDrawable(layers);
        mThumbnailView.setImageDrawable(transitionDrawable);
        transitionDrawable.setCrossFadeEnabled(true);
        transitionDrawable.startTransition(duration);
    }

    private static String getPublisherString(SnippetArticle suggestion) {
        // We format the publisher here so that having a publisher name in an RTL language
        // doesn't mess up the formatting on an LTR device and vice versa.
        return BidiFormatter.getInstance().unicodeWrap(suggestion.mPublisher);
    }

    private String getArticleAge(SnippetArticle suggestion) {
        if (suggestion.mPublishTimestampMilliseconds == 0) return "";

        CharSequence relativeTimeSpan;
        // DateUtils.getRelativeTimeSpanString(...) calls through to TimeZone.getDefault(). If this
        // has never been called before it loads the current time zone from disk. In most likelihood
        // this will have been called previously and the current time zone will have been cached,
        // but in some cases (eg instrumentation tests) it will cause a strict mode violation.
        // TODO(crbug.com/640210): Temporarily allowing disk access until more permanent fix is in.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            relativeTimeSpan =
                    DateUtils.getRelativeTimeSpanString(suggestion.mPublishTimestampMilliseconds,
                            System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS);
        }

        // We add a dash before the elapsed time, e.g. " - 14 minutes ago".
        return String.format(getArticleAgeFormatString(),
                BidiFormatter.getInstance().unicodeWrap(relativeTimeSpan));
    }

    private class FetchThumbnailCallback implements Callback<Bitmap> {
        private final SnippetArticle mCapturedSuggestion;
        private final int mThumbnailSize;

        FetchThumbnailCallback(SnippetArticle suggestion, int size) {
            mCapturedSuggestion = suggestion;
            mThumbnailSize = size;
        }

        @Override
        public void onResult(Bitmap thumbnail) {
            if (thumbnail == null) return; // Nothing to do, we keep the placeholder.

            // We need to crop and scale the downloaded bitmap, as the ImageView we set it on won't
            // be able to do so when using a TransitionDrawable (as opposed to the straight bitmap).
            // That's a limitation of TransitionDrawable, which doesn't handle layers of varying
            // sizes.
            if (thumbnail.getHeight() != mThumbnailSize || thumbnail.getWidth() != mThumbnailSize) {
                // Resize the thumbnail. If the provided bitmap is not cached or used anywhere else
                // (that's true for bitmaps returned by SuggestionsSource for ARTICLE
                // suggestions but not for those returned by ThumbnailProvider for DOWNLOADS for
                // example), recycle the input image in the process.
                thumbnail = ThumbnailUtils.extractThumbnail(thumbnail, mThumbnailSize,
                        mThumbnailSize,
                        mCapturedSuggestion.isArticle() ? ThumbnailUtils.OPTIONS_RECYCLE_INPUT : 0);
            }

            Drawable drawable = createThumbnailDrawable(thumbnail);

            // If the device has sufficient memory, store the image to skip the download task
            // next time we display this snippet.
            if (!SysUtils.isLowEndDevice()) {
                mCapturedSuggestion.setThumbnail(mUiDelegate.getReferencePool().put(drawable));
            }

            // Check whether the suggestions currently displayed in the view holder is the same as
            // the suggestion whose thumbnail we have just fetched.
            // This approach allows us to save the thumbnail in its corresponding SnippetArticle
            // regardless of whether a new suggestion has been bound to the view holder. This way we
            // don't have to cancel fetches and can use the retrieved thumbnail later on.
            if (mSuggestion == null
                    || !TextUtils.equals(mCapturedSuggestion.getUrl(), mSuggestion.getUrl())) {
                return;
            }

            fadeThumbnailIn(drawable);
        }
    }

    /**
     * Called when the containing view holder is recycled, to release unused resources.
     * @see NewTabPageViewHolder#recycle()
     */
    public void recycle() {
        // Clear the thumbnail and favicon drawables to allow the bitmap memory to be reclaimed.
        mThumbnailView.setImageDrawable(null);
        getPublisherIconTextView().setCompoundDrawables(null, null, null, null);

        mSuggestion = null;
    }

    private void verifyBitmap(Bitmap bitmap) {
        assert !bitmap.isRecycled();
        assert bitmap.getWidth() <= mThumbnailSize || bitmap.getHeight() <= mThumbnailSize;
    }

    /**
     * Get the size, shared width and height, for article thumbnails. This value will be used to
     * configure sizes of the thumbnails's {@link ImageView} and other elements within the card.
     * @return The size of the thumbnail.
     */
    protected int getThumbnailSize() {
        return mCardContainerView.getResources().getDimensionPixelSize(
                R.dimen.snippets_thumbnail_size);
    }

    /**
     * Get the size, shared width and height, for the publisher favicon. This value will be used to
     * configure the sizes of the rest of the publisher bar.
     * @return The size of the publisher favicon.
     */
    protected int getPublisherFaviconSizePx() {
        // The favicon of the publisher should match the TextView height.
        int widthSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        int heightSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        getPublisherIconTextView().measure(widthSpec, heightSpec);
        return getPublisherIconTextView().getMeasuredHeight();
    }

    /**
     * @return Formattable string with one %s to insert the age text.
     */
    protected String getArticleAgeFormatString() {
        return ARTICLE_AGE_FORMAT_STRING;
    }

    /**
     * @return The {@link TextView} that will be used to contain the publisher favicon.
     */
    protected TextView getPublisherIconTextView() {
        return mPublisherTextView;
    }

    /**
     * Create the {@link Drawable} for article thumbnail, from a simple {@link Bitmap}.
     * @param thumbnail The fetched thumbnail image for the article.
     * @return A {@link Drawable} to give to the view.
     */
    protected Drawable createThumbnailDrawable(Bitmap thumbnail) {
        return ThumbnailGradient.createDrawableWithGradientIfNeeded(
                thumbnail, mThumbnailView.getResources());
    }
}
