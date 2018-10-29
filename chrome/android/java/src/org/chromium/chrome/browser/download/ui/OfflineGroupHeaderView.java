// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageView;
import android.text.format.DateUtils;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.ui.DownloadHistoryAdapter.SubsectionHeader;
import org.chromium.chrome.browser.download.ui.DownloadItemSelectionDelegate.SubsectionHeaderSelectionObserver;
import org.chromium.chrome.browser.widget.DateDividedAdapter.TimedItem;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;
import org.chromium.chrome.download.R;

import java.util.Set;

/**
 * A header that presents users the option to view or hide the suggested offline pages.
 */
public class OfflineGroupHeaderView
        extends SelectableItemView<TimedItem> implements SubsectionHeaderSelectionObserver {
    private final int mIconBackgroundResId;
    private final ColorStateList mIconForegroundColorList;
    private final ColorStateList mCheckedIconForegroundColorList;

    private SubsectionHeader mHeader;
    private DownloadHistoryAdapter mAdapter;
    private DownloadItemSelectionDelegate mSelectionDelegate;

    private TextView mDescriptionTextView;
    private ImageView mExpandImage;
    private AppCompatImageView mIconImageView;

    public OfflineGroupHeaderView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mCheckedIconForegroundColorList = DownloadUtils.getIconForegroundColorList(context);
        mIconBackgroundResId = R.drawable.list_item_icon_modern_bg;

        mIconForegroundColorList =
                AppCompatResources.getColorStateList(context, R.color.dark_mode_tint);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconImageView = (AppCompatImageView) findViewById(R.id.icon_view);
        mDescriptionTextView = (TextView) findViewById(R.id.description);
        mExpandImage = (ImageView) findViewById(R.id.expand_icon);
    }

    /**
     * @param adapter The adapter associated with this header.
     */
    public void setAdapter(DownloadHistoryAdapter adapter) {
        mAdapter = adapter;
    }

    @Override
    public void setChecked(boolean checked) {
        if (checked == isChecked()) return;
        super.setChecked(checked);
        updateCheckIcon(checked);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mSelectionDelegate != null) {
            setChecked(mSelectionDelegate.isHeaderSelected(mHeader));
        }
    }

    /**
     * Updates the properties of this view.
     * @param header The associated {@link SubsectionHeader}.
     */
    @SuppressLint("StringFormatMatches")
    public void displayHeader(SubsectionHeader header) {
        this.mHeader = header;
        // TODO(crbug.com/635567): Fix lint properly.
        CharSequence timeSinceLastUpdate = DateUtils.getRelativeTimeSpanString(
                header.getTimestamp(), System.currentTimeMillis(), DateUtils.SECOND_IN_MILLIS);
        String totalFileSize = Formatter.formatFileSize(getContext(), header.getTotalFileSize());
        String description =
                getContext().getString(R.string.download_manager_offline_header_description,
                        totalFileSize, timeSinceLastUpdate);
        mDescriptionTextView.setText(description);
        updateExpandIcon(header.isExpanded());
        setChecked(mSelectionDelegate.isHeaderSelected(header));
        updateCheckIcon(isChecked());
    }

    private void updateExpandIcon(boolean expanded) {
        mExpandImage.setImageResource(expanded ? R.drawable.ic_expand_less_black_24dp
                                               : R.drawable.ic_expand_more_black_24dp);
        mExpandImage.setContentDescription(
                getResources().getString(expanded ? R.string.accessibility_collapse_section_header
                                                  : R.string.accessibility_expand_section_header));
    }

    private void updateCheckIcon(boolean checked) {
        if (checked) {
            mIconImageView.setBackgroundResource(mIconBackgroundResId);
            mIconImageView.getBackground().setLevel(
                    getResources().getInteger(R.integer.list_item_level_selected));

            mIconImageView.setImageDrawable(mCheckDrawable);
            ApiCompatibilityUtils.setImageTintList(mIconImageView, mCheckedIconForegroundColorList);
            mCheckDrawable.start();
        } else {
            mIconImageView.setBackgroundResource(mIconBackgroundResId);
            mIconImageView.getBackground().setLevel(
                    getResources().getInteger(R.integer.list_item_level_default));

            mIconImageView.setImageResource(R.drawable.ic_chrome);
            ApiCompatibilityUtils.setImageTintList(mIconImageView, mIconForegroundColorList);
        }
    }

    @Override
    public void onClick() {
        boolean newState = !mHeader.isExpanded();
        mAdapter.setPrefetchSectionExpanded(newState);
    }

    @Override
    protected boolean isSelectionModeActive() {
        return mSelectionDelegate.isSelectionEnabled();
    }

    @Override
    protected boolean toggleSelectionForItem(TimedItem item) {
        return mSelectionDelegate.toggleSelectionForSubsection(mHeader);
    }

    /**
     * Sets the selection delegate and registers |this| as
     * an observer. The delegate must be set before the item can respond to click events.
     * {@link SelectionDelegate} expects all the views to be of same type i.e.
     * SelectableItemView<DownloadHistoryItemWrapper>, whereas DownloadItemSelectionDelegate can
     * handle multiple types. This view being of type  SelectableItemView<TimedItem>, we need
     * to use a DownloadItemSelectionDelegate instead of SelectionDelegate.
     * @param delegate The selection delegate that will inform this item of selection changes.
     */
    public void setSelectionDelegate(DownloadItemSelectionDelegate delegate) {
        if (mSelectionDelegate == delegate) return;

        if (mSelectionDelegate != null) {
            mSelectionDelegate.removeObserver(this);
        }
        mSelectionDelegate = delegate;
        mSelectionDelegate.addObserver(this);
    }

    @Override
    public void onSubsectionHeaderSelectionStateChanged(Set<SubsectionHeader> selectedHeaders) {
        boolean isChecked = selectedHeaders.contains(mHeader);
        setChecked(isChecked);
    }
}
