// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView.ScaleType;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * The SelectableItemView for items displayed in the browsing history UI.
 */
public class HistoryItemView extends SelectableItemView<HistoryItem> {
    private ImageButton mRemoveButton;
    private VectorDrawableCompat mBlockedVisitDrawable;

    private final RoundedIconGenerator mIconGenerator;
    private DefaultFaviconHelper mFaviconHelper;

    private final int mMinIconSize;
    private final int mDisplayedIconSize;
    private final int mEndPadding;

    private boolean mRemoveButtonVisible;
    private boolean mIsItemRemoved;

    public HistoryItemView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mMinIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(context);
        mEndPadding =
                context.getResources().getDimensionPixelSize(R.dimen.default_list_row_padding);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mStartIconView.setImageResource(R.drawable.default_favicon);

        mRemoveButton = mEndButtonView;
        mRemoveButton.setImageResource(R.drawable.btn_delete_24dp);
        mRemoveButton.setContentDescription(getContext().getString((R.string.remove)));
        ImageViewCompat.setImageTintList(mRemoveButton,
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_icon_color_secondary_tint_list));
        mRemoveButton.setOnClickListener(v -> remove());
        mRemoveButton.setScaleType(ScaleType.CENTER_INSIDE);
        mRemoveButton.setPaddingRelative(
                getResources().getDimensionPixelSize(
                        R.dimen.history_item_remove_button_lateral_padding),
                getPaddingTop(),
                getResources().getDimensionPixelSize(
                        R.dimen.history_item_remove_button_lateral_padding),
                getPaddingBottom());

        updateRemoveButtonVisibility();
    }

    @Override
    public void setItem(HistoryItem item) {
        if (getItem() == item) return;

        super.setItem(item);

        mTitleView.setText(item.getTitle());
        mDescriptionView.setText(item.getDomain());
        SelectableListUtils.setContentDescriptionContext(getContext(), mRemoveButton,
                item.getTitle(), SelectableListUtils.ContentDescriptionSource.REMOVE_BUTTON);
        mIsItemRemoved = false;

        if (item.wasBlockedVisit()) {
            if (mBlockedVisitDrawable == null) {
                mBlockedVisitDrawable =
                        TraceEventVectorDrawableCompat.create(getContext().getResources(),
                                R.drawable.ic_block_red, getContext().getTheme());
            }
            setStartIconDrawable(mBlockedVisitDrawable);
            mTitleView.setTextColor(getContext().getColor(R.color.default_red));
        } else {
            setStartIconDrawable(mFaviconHelper.getDefaultFaviconDrawable(
                    getContext().getResources(), item.getUrl(), true));
            requestIcon();

            mTitleView.setTextColor(AppCompatResources.getColorStateList(
                    getContext(), R.color.default_text_color_list));
        }
    }

    /**
     * @param helper The helper for fetching default favicons.
     */
    public void setFaviconHelper(DefaultFaviconHelper helper) {
        mFaviconHelper = helper;
    }

    /**
     * Removes the item associated with this view.
     */
    public void remove() {
        // If the remove button is double tapped, this method may be called twice.
        if (getItem() == null || mIsItemRemoved) return;

        mIsItemRemoved = true;
        getItem().onItemRemoved();
    }

    /**
     * Should be called when the user's sign in state changes.
     */
    public void onSignInStateChange() {
        updateRemoveButtonVisibility();
    }

    /**
     * @param visible Whether the remove button should be visible. Note that this method will have
     *                no effect if the button is GONE because the signed in user is not allowed to
     *                delete browsing history.
     */
    public void setRemoveButtonVisible(boolean visible) {
        mRemoveButtonVisible = visible;
        if (!getPrefService().getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)) return;

        mRemoveButton.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void onClick() {
        if (getItem() != null) getItem().onItemClicked();
    }

    private void requestIcon() {
        HistoryItem item = getItem();
        if (item.wasBlockedVisit()) return;
        item.getLargeIconForUrl(
                mMinIconSize, (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                    // Prevent stale icons from making it through to the UI.
                    if (item != getItem()) return;

                    Drawable drawable = FaviconUtils.getIconDrawableWithoutFilter(icon,
                            getItem().getUrl(), fallbackColor, mIconGenerator, getResources(),
                            mDisplayedIconSize);
                    setStartIconDrawable(drawable);
                });
    }

    private void updateRemoveButtonVisibility() {
        int removeButtonVisibility =
                !getPrefService().getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)
                ? View.GONE
                : mRemoveButtonVisible ? View.VISIBLE : View.INVISIBLE;
        mRemoveButton.setVisibility(removeButtonVisibility);

        int endPadding = removeButtonVisibility == View.GONE ? mEndPadding : 0;
        ViewCompat.setPaddingRelative(mContentView, ViewCompat.getPaddingStart(mContentView),
                mContentView.getPaddingTop(), endPadding, mContentView.getPaddingBottom());
    }

    @VisibleForTesting
    View getRemoveButtonForTests() {
        return mRemoveButton;
    }

    private PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }
}
