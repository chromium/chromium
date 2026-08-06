// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.google.android.material.materialswitch.MaterialSwitch;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** View for the desktop android bookmark popup. */
@NullMarked
public class BookmarkPopupView extends ConstraintLayout {
    private ImageView mImageView;
    private TextView mHeaderTextView;
    private EditText mTitleView;
    private View mFolderPickerRow;
    private TextView mFolderTitle;
    private View mRemoveButton;
    private View mDoneButton;
    private View mCloseButton;

    private View mPriceTrackingContainer;
    private MaterialSwitch mPriceTrackingSwitch;

    /** Constructor for xml inflation. */
    public BookmarkPopupView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mImageView = findViewById(R.id.bookmark_image);
        mHeaderTextView = findViewById(R.id.popup_title);
        mTitleView = findViewById(R.id.bookmark_title);
        mFolderPickerRow = findViewById(R.id.folder_picker_row);
        mFolderTitle = findViewById(R.id.folder_title);
        mRemoveButton = findViewById(R.id.remove_button);
        mDoneButton = findViewById(R.id.done_button);
        mCloseButton = findViewById(R.id.close_button);
        mPriceTrackingContainer = findViewById(R.id.price_tracking_container);
        mPriceTrackingSwitch = findViewById(R.id.price_tracking_switch);
    }

    /** Sets the header text of the popup (e.g., "Bookmark added"). */
    public void setHeaderText(String headerText) {
        mHeaderTextView.setText(headerText);
    }

    /** Sets the bookmark title text in the editable title field. */
    public void setTitle(String title) {
        if (!TextUtils.equals(mTitleView.getText(), title)) {
            mTitleView.setText(title);
        }
    }

    private @Nullable TextWatcher mTitleTextWatcher;

    /** Sets the {@link TextWatcher} to observe title edits. */
    public void setTitleTextWatcher(@Nullable TextWatcher watcher) {
        if (mTitleTextWatcher != null) {
            mTitleView.removeTextChangedListener(mTitleTextWatcher);
        }
        mTitleTextWatcher = watcher;
        if (mTitleTextWatcher != null) {
            mTitleView.addTextChangedListener(mTitleTextWatcher);
        }
    }

    /** Sets the bookmark thumbnail image drawable. */
    public void setImageDrawable(@Nullable Drawable drawable) {
        mImageView.setImageDrawable(drawable);
    }

    /** Sets the scale type for the bookmark thumbnail image view. */
    public void setImageScaleType(ImageView.ScaleType scaleType) {
        mImageView.setScaleType(scaleType);
    }

    /** Sets the folder name displayed in the folder selector row. */
    public void setFolderName(String folderName) {
        mFolderTitle.setText(folderName);
        mFolderPickerRow.setContentDescription(
                getContext().getString(R.string.bookmark_choose_folder_with_name, folderName));
    }

    /** Sets the click listener for the folder selector row. */
    public void setFolderRowClickListener(@Nullable OnClickListener listener) {
        mFolderPickerRow.setOnClickListener(listener);
    }

    /** Sets the click listener for the Remove action button. */
    public void setRemoveClickListener(@Nullable OnClickListener listener) {
        mRemoveButton.setOnClickListener(listener);
    }

    /** Sets the click listener for the Done action button. */
    public void setDoneClickListener(@Nullable OnClickListener listener) {
        mDoneButton.setOnClickListener(listener);
    }

    /** Sets the click listener for the top right Close icon button. */
    public void setCloseClickListener(@Nullable OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    /** Sets the visibility of the price tracking section. */
    public void setPriceTrackingVisible(boolean visible) {
        mPriceTrackingContainer.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Sets if the price tracking section is enabled. */
    public void setPriceTrackingEnabled(boolean enabled) {
        mPriceTrackingContainer.setEnabled(enabled);
        mPriceTrackingSwitch.setEnabled(enabled);
    }

    /** Sets if the price tracking switch is checked. */
    public void setPriceTrackingSwitchChecked(boolean checked) {
        mPriceTrackingSwitch.setChecked(checked);
    }

    /** Sets the change listener for the price tracking switch. */
    public void setPriceTrackingSwitchListener(
            CompoundButton.@Nullable OnCheckedChangeListener listener) {
        mPriceTrackingSwitch.setOnCheckedChangeListener(listener);
    }

    /** Cleans up listeners and observers to prevent leaks. */
    public void destroy() {
        setTitleTextWatcher(null);
        mFolderPickerRow.setOnClickListener(null);
        mRemoveButton.setOnClickListener(null);
        mDoneButton.setOnClickListener(null);
        mCloseButton.setOnClickListener(null);
        mPriceTrackingSwitch.setOnCheckedChangeListener(null);
    }
}
