// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.url.GURL;

/**
 * A {@link MessageCardViewProperties#IconProvider} that obtains favicons asynchronously to create a
 * composed icon for a LargeMessageCard.
 */
public class MultiFaviconIconProvider implements MessageCardView.IconProvider {
    private final Context mContext;
    private final DefaultFaviconHelper mDefaultFaviconHelper;
    private final RoundedIconGenerator mIconGenerator;
    private final Profile mProfile;
    private final int mFaviconSize;
    private final int mFaviconInset;

    private FaviconHelper mFaviconHelper;
    private Callback<Drawable> mFinishedCallback;
    private Promise<Drawable> mFaviconLeft = new Promise<Drawable>();
    private Promise<Drawable> mFaviconCentre = new Promise<Drawable>();
    private Promise<Drawable> mFaviconRight = new Promise<Drawable>();
    private boolean mIsFinished;
    private Drawable mFinalIconDrawable;
    private GURL mLeftIconUrl;
    private GURL mCentreIconUrl;
    private GURL mRightIconUrl;

    public MultiFaviconIconProvider(Context context, TabSuggestion tabSuggestion, Profile profile) {
        this(context, tabSuggestion, profile, new FaviconHelper());
    }

    protected MultiFaviconIconProvider(
            Context context,
            TabSuggestion tabSuggestion,
            Profile profile,
            FaviconHelper faviconHelper) {
        mContext = context;
        mDefaultFaviconHelper = new DefaultFaviconHelper();
        mIconGenerator = FaviconUtils.createRoundedRectangleIconGenerator(mContext);
        mFaviconHelper = faviconHelper;
        mProfile = profile;
        mFaviconSize = mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mFaviconInset =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_favicon_inset);

        // Take the first 3 tabs in TabSuggestion for icon assembly.
        mLeftIconUrl = new GURL(tabSuggestion.getTabsInfo().get(0).url);
        mCentreIconUrl = new GURL(tabSuggestion.getTabsInfo().get(1).url);
        mRightIconUrl = new GURL(tabSuggestion.getTabsInfo().get(2).url);
    }

    @Override
    public void fetchIconDrawable(Callback<Drawable> callback) {
        // Retrieve the cached icon drawable if it's available on subsequent invocations.
        if (mFinalIconDrawable != null) {
            callback.onResult(mFinalIconDrawable);
            return;
        }

        assert mFinishedCallback == null : "Callback drawable should not have a value.";
        mFinishedCallback = callback;
        startFetching();
    }

    private void destroy() {
        mFaviconHelper.destroy();
        mFaviconHelper = null;
    }

    private void startFetching() {
        mFaviconLeft.then(this::maybeFinishFetching);
        mFaviconCentre.then(this::maybeFinishFetching);
        mFaviconRight.then(this::maybeFinishFetching);

        // Retrieve the favicon bitmaps and convert them to inset drawables.
        mFaviconHelper.getLocalFaviconImageForURL(
                mProfile,
                mLeftIconUrl,
                mFaviconSize,
                (bitmap, url) -> retrieveFavicon(mFaviconLeft, bitmap, mLeftIconUrl));
        mFaviconHelper.getLocalFaviconImageForURL(
                mProfile,
                mCentreIconUrl,
                mFaviconSize,
                (bitmap, url) -> retrieveFavicon(mFaviconCentre, bitmap, mCentreIconUrl));
        mFaviconHelper.getLocalFaviconImageForURL(
                mProfile,
                mRightIconUrl,
                mFaviconSize,
                (bitmap, url) -> retrieveFavicon(mFaviconRight, bitmap, mRightIconUrl));
    }

    private void maybeFinishFetching(Drawable drawable) {
        if (!mIsFinished
                && mFaviconLeft.isFulfilled()
                && mFaviconCentre.isFulfilled()
                && mFaviconRight.isFulfilled()) {
            finishFetching();
            mIsFinished = true;
        }
    }

    private void finishFetching() {
        int iconHeight =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_icon_height);
        int iconWidth =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_icon_width);
        int iconEnd1 =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_icon_end_1);
        int iconStart2 =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_icon_start_2);
        int iconEnd2 =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_icon_end_2);
        int iconStart3 =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.tab_cleanup_promo_card_icon_start_3);

        // Retrieve the background drawable.
        Drawable bg =
                AppCompatResources.getDrawable(
                        mContext, R.drawable.tab_cleanup_message_card_icon_bg);

        // Create the bitmap.
        Bitmap bitmap = Bitmap.createBitmap(iconWidth, iconHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        // Set the background drawable.
        bg.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        bg.draw(canvas);

        // Arrange the favicon drawables.
        mFaviconLeft.getResult().setBounds(0, 0, iconEnd1, canvas.getHeight());
        mFaviconLeft.getResult().draw(canvas);
        mFaviconCentre.getResult().setBounds(iconStart2, 0, iconEnd2, canvas.getHeight());
        mFaviconCentre.getResult().draw(canvas);
        mFaviconRight.getResult().setBounds(iconStart3, 0, canvas.getWidth(), canvas.getHeight());
        mFaviconRight.getResult().draw(canvas);

        // Set the finished icon drawable.
        mFinalIconDrawable = new BitmapDrawable(mContext.getResources(), bitmap);
        mFinishedCallback.onResult(mFinalIconDrawable);

        // Clean up.
        mFinishedCallback = null;
        destroy();
    }

    private void retrieveFavicon(Promise<Drawable> promise, Bitmap bitmap, GURL url) {
        Drawable favicon =
                new InsetDrawable(
                        FaviconUtils.getIconDrawableWithFilter(
                                bitmap,
                                url,
                                mIconGenerator,
                                mDefaultFaviconHelper,
                                mContext,
                                mFaviconSize),
                        mFaviconInset);
        promise.fulfill(favicon);
    }
}
