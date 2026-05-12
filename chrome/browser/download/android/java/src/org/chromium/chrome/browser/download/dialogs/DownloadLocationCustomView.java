// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.TextView;

import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadLocationDialogMetrics;
import org.chromium.chrome.browser.download.DownloadLocationDialogMetrics.DownloadLocationSuggestionEvent;
import org.chromium.chrome.browser.download.DownloadLocationDialogType;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.StringUtils;
import org.chromium.components.browser_ui.widget.text.AlertDialogEditText;

/** Dialog that is displayed to ask user where they want to download the file. */
@NullMarked
public class DownloadLocationCustomView extends ScrollView implements OnCheckedChangeListener {

    private TextView mTitle;
    private TextView mSubtitleView;
    private TextView mIncognitoWarning;
    private AlertDialogEditText mFileName;
    private TextView mFileSize;
    private Spinner mFileLocation;
    private TextView mLocationAvailableSpace;
    private CheckBox mDontShowAgain;
    private @DownloadLocationDialogType int mDialogType;
    private long mTotalBytes;
    private @Nullable Callback<Boolean> mOnClickedCallback;

    public DownloadLocationCustomView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitle = findViewById(R.id.title);
        mSubtitleView = findViewById(R.id.subtitle);
        mIncognitoWarning = findViewById(R.id.incognito_warning);
        mFileName = findViewById(R.id.file_name);
        mFileSize = findViewById(R.id.file_size);
        mFileLocation = findViewById(R.id.file_location);
        mLocationAvailableSpace = findViewById(R.id.location_available_space);
        mDontShowAgain = findViewById(R.id.show_again_checkbox);
    }

    void initialize(
            @DownloadLocationDialogType int dialogType,
            long totalBytes,
            Callback<Boolean> onClickedCallback) {
        // TODO(xingliu): Remove this function, currently used by smart suggestion.
        mDialogType = dialogType;
        mTotalBytes = totalBytes;
        mOnClickedCallback = onClickedCallback;
    }

    void setTitle(CharSequence title) {
        mTitle.setText(title);
    }

    void setSubtitle(CharSequence subtitle) {
        mSubtitleView.setText(subtitle);
    }

    void setFileName(CharSequence fileName) {
        mFileName.setText(fileName);
    }

    void setFileSize(CharSequence fileSize) {
        mFileSize.setVisibility(View.VISIBLE);
        mFileSize.setText(fileSize);
    }

    void setDontShowAgainCheckbox(boolean checked) {
        mDontShowAgain.setChecked(checked);
        mDontShowAgain.setOnCheckedChangeListener(this);
    }

    void showIncognitoWarning(boolean show) {
        mIncognitoWarning.setVisibility(show ? VISIBLE : GONE);
    }

    void showDontShowAgainCheckbox(boolean show) {
        mDontShowAgain.setVisibility(show ? VISIBLE : GONE);
    }

    void showLocationAvailableSpace(boolean show) {
        mLocationAvailableSpace.setVisibility(show ? VISIBLE : GONE);
    }

    // CompoundButton.OnCheckedChangeListener implementation.
    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        assumeNonNull(mOnClickedCallback);
        mOnClickedCallback.onResult(isChecked);
    }

    // Helper methods available to DownloadDialogBridge.
    /**
     * @return The text that the user inputted as the name of the file.
     */
    @Nullable String getFileName() {
        if (mFileName == null || mFileName.getText() == null) return null;
        return mFileName.getText().toString();
    }

    /**
     * @return The file path based on what the user selected as the location of the file.
     */
    @Nullable DirectoryOption getDirectoryOption() {
        if (mFileLocation == null) return null;
        DirectoryOption selected = (DirectoryOption) mFileLocation.getSelectedItem();
        return selected;
    }

    /**
     * @return Whether the "don't show again" checkbox is checked.
     */
    boolean getDontShowAgain() {
        return mDontShowAgain != null && mDontShowAgain.isChecked();
    }

    /** Hide the subtitle and adjust the bottom margin. */
    void showSubtitle(boolean show) {
        mSubtitleView.setVisibility(show ? View.VISIBLE : View.GONE);

        MarginLayoutParams titleMargin = (MarginLayoutParams) mTitle.getLayoutParams();
        titleMargin.bottomMargin =
                getResources()
                        .getDimensionPixelSize(
                                show
                                        ? R.dimen.download_dialog_title_margin_bottom
                                        : R.dimen.download_dialog_subtitle_margin_bottom);
        mTitle.setLayoutParams(titleMargin);
    }

    /**
     * Show the available space below the file location spinner.
     * @param  availableSpace The available space of the file location.
     */
    void setLocationAvailableSpace(long availableSpace) {
        if (mDialogType != DownloadLocationDialogType.LOCATION_SUGGESTION) return;
        String locationAvailableSpaceText =
                StringUtils.getAvailableBytesForUi(getContext(), availableSpace);
        ColorStateList textColor = getContext().getColorStateList(R.color.default_text_color_list);
        int barColor = ContextCompat.getColor(getContext(), R.color.explanation_text_color);

        // Show not enough space and change color to error.
        if (availableSpace < mTotalBytes) {
            locationAvailableSpaceText =
                    getContext()
                            .getString(
                                    R.string.download_manager_list_item_description,
                                    locationAvailableSpaceText,
                                    getContext()
                                            .getText(R.string.download_location_not_enough_space));
            textColor =
                    ColorStateList.valueOf(
                            ContextCompat.getColor(
                                    getContext(), R.color.input_underline_error_color));
            barColor = ContextCompat.getColor(getContext(), R.color.input_underline_error_color);

            DownloadLocationDialogMetrics.recordDownloadLocationSuggestionEvent(
                    DownloadLocationSuggestionEvent.NOT_ENOUGH_SPACE_SHOWN);
        }

        mLocationAvailableSpace.setText(locationAvailableSpaceText);
        mLocationAvailableSpace.setTextColor(textColor);
        DrawableCompat.setTint(mFileLocation.getBackground().mutate(), barColor);
    }

    /** Sets the adapter and selection for the file location spinner. */
    void setFileLocationSpinner(SpinnerAdapter adapter, int selectedItemId) {
        mFileLocation.setAdapter(adapter);
        mFileLocation.setSelection(selectedItemId);
    }

    /** Sets the item selected listener for the file location spinner. */
    void setFileLocationSpinnerListener(AdapterView.OnItemSelectedListener listener) {
        mFileLocation.setOnItemSelectedListener(listener);
    }
}
