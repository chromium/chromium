// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.media.ThumbnailUtils;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.v4.text.BidiFormatter;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.ui.DownloadFilter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.util.ViewUtils;

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
    private static final int MAX_SNIPPET_LINES_ALTERNATE = 4;

    private final ImageFetcher mImageFetcher;
    private final SuggestionsUiDelegate mUiDelegate;
    private final boolean mIsContextual;
    private final boolean mUseContextualAlternateCardLayout;

    private final View mCardContainerView;
    private final LinearLayout mTextLayout;
    private final TextView mHeadlineTextView;
    private final @Nullable TextView mSnippetTextView;
    private final TextView mPublisherTextView;
    private final TextView mAgeTextView;
    // TODO(twellington): Try to change this back to a TintedImageView. This was changed for a crash
    // in contextual suggestions that occurs when trying to mutate state when tinting a
    // LayerDrawable that contains a RoundedBitmapDrawable on older versions of Android.
    private final ImageView mThumbnailView;
    private final @Nullable ImageView mVideoBadge;
    private final ImageView mOfflineBadge;
    private final View mPublisherBar;
    private final int mThumbnailSize;
    private final int mSmallThumbnailCornerRadius;

    boolean mShowThumbnail;
    boolean mHasVideoBadge;
    boolean mHasOfflineBadge;

    @Nullable
    private ImageFetcher.DownloadThumbnailRequest mThumbnailRequest;

    private SnippetArticle mSuggestion;

    /**
     * Creates a new SuggestionsBinder.
     * @param cardContainerView The root container view for the card.
     * @param uiDelegate The interface between the suggestion surface and the rest of the browser.
     * @param isContextual Whether this binder is used to bind contextual suggestions.
     */
    public SuggestionsBinder(
            View cardContainerView, SuggestionsUiDelegate uiDelegate, boolean isContextual) {
        mIsContextual = isContextual;
        mCardContainerView = cardContainerView;
        mUiDelegate = uiDelegate;
        mImageFetcher = uiDelegate.getImageFetcher();

        mTextLayout = mCardContainerView.findViewById(R.id.text_layout);
        mThumbnailView = mCardContainerView.findViewById(R.id.article_thumbnail);
        mHeadlineTextView = mCardContainerView.findViewById(R.id.article_headline);
        mSnippetTextView = mCardContainerView.findViewById(R.id.article_snippet);
        mPublisherTextView = mCardContainerView.findViewById(R.id.article_publisher);
        mAgeTextView = mCardContainerView.findViewById(R.id.article_age);
        mVideoBadge = mCardContainerView.findViewById(R.id.video_badge);
        mOfflineBadge = mCardContainerView.findViewById(R.id.offline_icon);
        mPublisherBar = mCardContainerView.findViewById(R.id.publisher_bar);

        if (mIsContextual) {
            mThumbnailSize = mCardContainerView.getResources().getDimensionPixelSize(
                    R.dimen.snippets_thumbnail_size_small);
            mUseContextualAlternateCardLayout = ChromeFeatureList.isEnabled(
                    ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_ALTERNATE_CARD_LAYOUT);
        } else {
            mThumbnailSize = getThumbnailSize(mCardContainerView.getResources());
            mUseContextualAlternateCardLayout = false;
        }

        mSmallThumbnailCornerRadius = mCardContainerView.getResources().getDimensionPixelSize(
                R.dimen.snippets_thumbnail_small_corner_radius);
    }

    public void updateViewInformation(SnippetArticle suggestion) {
        assert suggestion.isContextual() == mIsContextual;

        mSuggestion = suggestion;

        mHeadlineTextView.setText(suggestion.mTitle);
        mPublisherTextView.setText(getPublisherString(suggestion));
        mAgeTextView.setText(getArticleAge(suggestion));

        setFavicon();
        setThumbnail();

        if (mSnippetTextView != null) {
            mSnippetTextView.setText(suggestion.mSnippet);
        }
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

        ViewGroup.MarginLayoutParams publisherBarParams =
                (ViewGroup.MarginLayoutParams) mPublisherBar.getLayoutParams();

        if (showHeadline && !mUseContextualAlternateCardLayout) {
            // When we show a headline and not a description, we reduce the top margin of the
            // publisher bar.
            publisherBarParams.topMargin = mPublisherBar.getResources().getDimensionPixelSize(
                    R.dimen.snippets_publisher_margin_top);
        } else {
            // When there is no headline and no description, we remove the top margin of the
            // publisher bar.
            publisherBarParams.topMargin = 0;
        }

        mTextLayout.setMinimumHeight(showThumbnail ? mThumbnailSize : 0);
        mPublisherBar.setLayoutParams(publisherBarParams);

        if (mSnippetTextView != null) {
            mSnippetTextView.setVisibility(showSnippet ? View.VISIBLE : View.GONE);
            mSnippetTextView.setMaxLines(mUseContextualAlternateCardLayout
                            ? MAX_SNIPPET_LINES_ALTERNATE
                            : MAX_SNIPPET_LINES);
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

        mOfflineBadge.setVisibility(mHasOfflineBadge ? View.VISIBLE : View.GONE);
    }

    private void setFavicon() {
        // The favicon of the publisher should match the TextView height.
        int widthSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        int heightSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        mPublisherTextView.measure(widthSpec, heightSpec);
        int publisherFaviconSizePx = mPublisherTextView.getMeasuredHeight();

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
            Drawable drawable =
                    new BitmapDrawable(mPublisherTextView.getContext().getResources(), bitmap);
            // If the device has sufficient memory, store the favicon to skip the download task
            // next time we display this snippet.
            if (!SysUtils.isLowEndDevice() && mSuggestion != null) {
                mSuggestion.setPublisherFavicon(mUiDelegate.getReferencePool().put(drawable));
            }
            setFaviconOnView(drawable, publisherFaviconSizePx);
        };

        mImageFetcher.makeFaviconRequest(mSuggestion, publisherFaviconSizePx, faviconCallback);
    }

    private void setThumbnail() {
        // If there's still a pending thumbnail fetch, cancel it.
        cancelThumbnailFetch();

        // mThumbnailView's visibility is modified in updateFieldsVisibility().
        if (mThumbnailView.getVisibility() != View.VISIBLE) return;

        Drawable thumbnail = mSuggestion.getThumbnail();
        if (thumbnail != null) {
            setThumbnail(thumbnail);
            return;
        }

        if (mSuggestion.isDownload()) {
            setDownloadThumbnail();
            return;
        }

        // Temporarily set placeholder and then fetch the thumbnail from a provider.
        mThumbnailView.setBackground(null);
        if (mIsContextual) {
            mThumbnailView.setImageResource(mUseContextualAlternateCardLayout
                            ? R.drawable.contextual_suggestions_placeholder_thumbnail_alternate
                            : R.drawable.contextual_suggestions_placeholder_thumbnail);
        } else if (ChromeFeatureList.isEnabled(
                           ChromeFeatureList.CONTENT_SUGGESTIONS_THUMBNAIL_DOMINANT_COLOR)) {
            ColorDrawable colorDrawable =
                    new ColorDrawable(mSuggestion.getThumbnailDominantColor() != null
                                    ? mSuggestion.getThumbnailDominantColor()
                                    : ApiCompatibilityUtils.getColor(mThumbnailView.getResources(),
                                              R.color.modern_grey_100));
            mThumbnailView.setImageDrawable(colorDrawable);
        } else {
            mThumbnailView.setImageResource(R.drawable.ic_snippet_thumbnail_placeholder);
        }
        if (!mIsContextual) ApiCompatibilityUtils.setImageTintList(mThumbnailView, null);

        // Fetch thumbnail for the current article.
        mImageFetcher.makeArticleThumbnailRequest(
                mSuggestion, new FetchThumbnailCallback(mSuggestion, mThumbnailSize));
    }

    private void setDownloadThumbnail() {
        assert mSuggestion.isDownload();
        if (!mSuggestion.isAssetDownload()) {
            setThumbnailFromFileType(DownloadFilter.Type.PAGE);
            return;
        }

        @DownloadFilter.Type
        int fileType = DownloadFilter.fromMimeType(mSuggestion.getAssetDownloadMimeType());
        if (fileType == DownloadFilter.Type.IMAGE) {
            // For image downloads, attempt to fetch a thumbnail.
            ImageFetcher.DownloadThumbnailRequest thumbnailRequest =
                    mImageFetcher.makeDownloadThumbnailRequest(mSuggestion, mThumbnailSize);

            Promise<Bitmap> thumbnailReceivedPromise = thumbnailRequest.getPromise();

            if (thumbnailReceivedPromise.isFulfilled()) {
                // If the thumbnail was cached, then it will be retrieved synchronously, the promise
                // will be fulfilled and we can set the thumbnail immediately.
                verifyBitmap(thumbnailReceivedPromise.getResult());
                setThumbnail(ThumbnailGradient.createDrawableWithGradientIfNeeded(
                        thumbnailReceivedPromise.getResult(), mCardContainerView.getResources()));

                return;
            }

            mThumbnailRequest = thumbnailRequest;

            // Queue a callback to be called after the thumbnail is retrieved asynchronously.
            thumbnailReceivedPromise.then(new FetchThumbnailCallback(mSuggestion, mThumbnailSize));
        }

        // Set a placeholder for the file type.
        setThumbnailFromFileType(fileType);
    }

    private void setThumbnail(Drawable thumbnail) {
        assert thumbnail != null;

        mThumbnailView.setScaleType(ImageView.ScaleType.CENTER_CROP);
        mThumbnailView.setBackground(null);
        mThumbnailView.setImageDrawable(thumbnail);
        if (!mIsContextual) ApiCompatibilityUtils.setImageTintList(mThumbnailView, null);
    }

    private void setThumbnailFromFileType(@DownloadFilter.Type int fileType) {
        int iconBackgroundColor = DownloadUtils.getIconBackgroundColor(mThumbnailView.getContext());
        ColorStateList iconForegroundColorList =
                DownloadUtils.getIconForegroundColorList(mThumbnailView.getContext());

        mThumbnailView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        mThumbnailView.setBackgroundColor(iconBackgroundColor);
        mThumbnailView.setImageResource(
                DownloadUtils.getIconResId(fileType, DownloadUtils.IconSize.DP_36));
        if (!mIsContextual) {
            ApiCompatibilityUtils.setImageTintList(mThumbnailView, iconForegroundColorList);
        }
    }

    private void setDefaultFaviconOnView(int faviconSizePx) {
        setFaviconOnView(
                ApiCompatibilityUtils.getDrawable(
                        mPublisherTextView.getContext().getResources(), R.drawable.default_favicon),
                faviconSizePx);
    }

    private void setFaviconOnView(Drawable drawable, int faviconSizePx) {
        drawable.setBounds(0, 0, faviconSizePx, faviconSizePx);
        ApiCompatibilityUtils.setCompoundDrawablesRelative(
                mPublisherTextView, drawable, null, null, null);
        mPublisherTextView.setVisibility(View.VISIBLE);
    }

    private void cancelThumbnailFetch() {
        if (mThumbnailRequest != null) {
            mThumbnailRequest.cancel();
            mThumbnailRequest = null;
        }
    }

    private void fadeThumbnailIn(Drawable thumbnail) {
        assert mThumbnailView.getDrawable() != null;

        mThumbnailView.setScaleType(ImageView.ScaleType.CENTER_CROP);
        mThumbnailView.setBackground(null);
        if (!mIsContextual) ApiCompatibilityUtils.setImageTintList(mThumbnailView, null);
        int duration = (int) (FADE_IN_ANIMATION_TIME_MS
                * ChromeAnimation.Animation.getAnimationMultiplier());
        if (duration == 0) {
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

    private static String getArticleAge(SnippetArticle suggestion) {
        if (suggestion.mPublishTimestampMilliseconds == 0) return "";

        // DateUtils.getRelativeTimeSpanString(...) calls through to TimeZone.getDefault(). If this
        // has never been called before it loads the current time zone from disk. In most likelihood
        // this will have been called previously and the current time zone will have been cached,
        // but in some cases (eg instrumentation tests) it will cause a strict mode violation.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        CharSequence relativeTimeSpan;
        try {
            long time = SystemClock.elapsedRealtime();
            relativeTimeSpan =
                    DateUtils.getRelativeTimeSpanString(suggestion.mPublishTimestampMilliseconds,
                            System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS);
            SuggestionsMetrics.recordDateFormattingDuration(SystemClock.elapsedRealtime() - time);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        // We add a dash before the elapsed time, e.g. " - 14 minutes ago".
        return String.format(ARTICLE_AGE_FORMAT_STRING,
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

            Drawable drawable;
            if (mIsContextual) {
                drawable = mUseContextualAlternateCardLayout
                        ? new BitmapDrawable(mThumbnailView.getResources(), thumbnail)
                        : ViewUtils.createRoundedBitmapDrawable(
                                  thumbnail, mSmallThumbnailCornerRadius);
            } else {
                drawable = ThumbnailGradient.createDrawableWithGradientIfNeeded(
                        thumbnail, mThumbnailView.getResources());
            }

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
        mPublisherTextView.setCompoundDrawables(null, null, null, null);

        mSuggestion = null;
    }

    private void verifyBitmap(Bitmap bitmap) {
        assert !bitmap.isRecycled();
        assert bitmap.getWidth() <= mThumbnailSize || bitmap.getHeight() <= mThumbnailSize;
    }

    private static int getThumbnailSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.snippets_thumbnail_size);
    }
}
