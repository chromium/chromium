// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.chromium.chrome.browser.download.settings.DownloadDirectoryAdapter.NO_SELECTED_ITEM_ID;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadLocationDialogMetrics;
import org.chromium.chrome.browser.download.DownloadLocationDialogMetrics.DownloadLocationSuggestionEvent;
import org.chromium.chrome.browser.download.DownloadLocationDialogType;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.StringUtils;
import org.chromium.chrome.browser.download.settings.DownloadDirectoryAdapter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.browser_ui.widget.text.AlertDialogEditText;

import java.io.File;

/**
 * Dialog that is displayed to ask user where they want to download the file.
 */
public class DownloadLocationCustomView
        extends ScrollView implements OnCheckedChangeListener, DownloadDirectoryAdapter.Delegate {
    private DownloadDirectoryAdapter mDirectoryAdapter;

    private TextView mTitle;
    private TextView mSubtitleView;
    private AlertDialogEditText mFileName;
    private TextView mFileSize;
    private Spinner mFileLocation;
    private TextView mLocationAvailableSpace;
    private CheckBox mDontShowAgain;
    private @DownloadLocationDialogType int mDialogType;
    private long mTotalBytes;

    public DownloadLocationCustomView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDirectoryAdapter = new DownloadDirectoryAdapter(context, this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitle = findViewById(R.id.title);
        mSubtitleView = findViewById(R.id.subtitle);
        mFileName = findViewById(R.id.file_name);
        mFileSize = findViewById(R.id.file_size);
        mFileLocation = findViewById(R.id.file_location);
        mLocationAvailableSpace = findViewById(R.id.location_available_space);
        mDontShowAgain = findViewById(R.id.show_again_checkbox);
    }

    void initialize(@DownloadLocationDialogType int dialogType, File suggestedPath, long totalBytes,
            CharSequence title) {
        mDialogType = dialogType;

        // Automatically check "don't show again" the first time the user is seeing the dialog.
        boolean isInitial = DownloadDialogBridge.getPromptForDownloadAndroid()
                == DownloadPromptStatus.SHOW_INITIAL;
        mDontShowAgain.setChecked(isInitial);
        mDontShowAgain.setOnCheckedChangeListener(this);

        mFileName.setText(suggestedPath.getName());
        mTitle.setText(title);
        mTotalBytes = totalBytes;

        switch (dialogType) {
            case DownloadLocationDialogType.DEFAULT:
                // Show a file size subtitle if file size is available.
                if (totalBytes > 0) {
                    mSubtitleView.setText(
                            DownloadUtils.getStringForBytes(getContext(), totalBytes));
                } else {
                    hideSubtitle();
                }
                break;

            case DownloadLocationDialogType.LOCATION_FULL:
                mSubtitleView.setText(R.string.download_location_download_to_default_folder);
                break;

            case DownloadLocationDialogType.LOCATION_NOT_FOUND:
                mSubtitleView.setText(R.string.download_location_download_to_default_folder);
                break;

            case DownloadLocationDialogType.NAME_CONFLICT:
                mSubtitleView.setText(R.string.download_location_name_exists);
                break;

            case DownloadLocationDialogType.NAME_TOO_LONG:
                mSubtitleView.setText(R.string.download_location_name_too_long);
                break;

            case DownloadLocationDialogType.LOCATION_SUGGESTION:
                // Show the location available space underneath the spinner.
                mLocationAvailableSpace.setVisibility(View.VISIBLE);
                setFileSize(totalBytes);
                hideSubtitle();
                break;
        }

        mDirectoryAdapter.update();
    }

    // CompoundButton.OnCheckedChangeListener implementation.
    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        DownloadDialogBridge.setPromptForDownloadAndroid(
                isChecked ? DownloadPromptStatus.DONT_SHOW : DownloadPromptStatus.SHOW_PREFERENCE);
    }

    // Helper methods available to DownloadDialogBridge.
    /**
     * @return  The text that the user inputted as the name of the file.
     */
    @Nullable
    String getFileName() {
        if (mFileName == null || mFileName.getText() == null) return null;
        return mFileName.getText().toString();
    }

    /**
     * @return  The file path based on what the user selected as the location of the file.
     */
    @Nullable
    DirectoryOption getDirectoryOption() {
        if (mFileLocation == null) return null;
        DirectoryOption selected = (DirectoryOption) mFileLocation.getSelectedItem();
        return selected;
    }

    /**
     * @return  Whether the "don't show again" checkbox is checked.
     */
    boolean getDontShowAgain() {
        return mDontShowAgain != null && mDontShowAgain.isChecked();
    }

    /**
     * Hide the subtitle and adjust the bottom margin.
     */
    private void hideSubtitle() {
        mSubtitleView.setVisibility(View.GONE);
        MarginLayoutParams titleMargin = (MarginLayoutParams) mTitle.getLayoutParams();
        titleMargin.bottomMargin = getResources().getDimensionPixelSize(
                R.dimen.download_dialog_subtitle_margin_bottom);
        mTitle.setLayoutParams(titleMargin);
    }

    /**
     * Show the file size below the file name.
     * @param  totalBytes The total bytes of the download.
     */
    private void setFileSize(long totalBytes) {
        mFileSize.setVisibility(View.VISIBLE);
        mFileSize.setText(DownloadUtils.getStringForBytes(getContext(), totalBytes));
    }

    /**
     * Show the available space below the file location spinner.
     * @param  availableSpace The available space of the file location.
     */
    private void setLocationAvailableSpace(long availableSpace) {
        if (mDialogType != DownloadLocationDialogType.LOCATION_SUGGESTION) return;
        String locationAvailableSpaceText =
                StringUtils.getAvailableBytesForUi(getContext(), availableSpace);
        int textColor = ContextCompat.getColor(getContext(), R.color.default_text_color);
        int barColor = ContextCompat.getColor(getContext(), R.color.explanation_text_color);

        // Show not enough space and change color to error.
        if (availableSpace < mTotalBytes) {
            locationAvailableSpaceText = getContext().getResources().getString(
                    R.string.download_manager_list_item_description, locationAvailableSpaceText,
                    getContext().getText(R.string.download_location_not_enough_space));
            textColor = ContextCompat.getColor(getContext(), R.color.input_underline_error_color);
            barColor = ContextCompat.getColor(getContext(), R.color.input_underline_error_color);

            DownloadLocationDialogMetrics.recordDownloadLocationSuggestionEvent(
                    DownloadLocationSuggestionEvent.NOT_ENOUGH_SPACE_SHOWN);
        }

        mLocationAvailableSpace.setText(locationAvailableSpaceText);
        mLocationAvailableSpace.setTextColor(textColor);
        DrawableCompat.setTint(mFileLocation.getBackground().mutate(), barColor);
    }

    // DownloadDirectoryAdapter.Delegate implementation.
    @Override
    public void onDirectoryOptionsUpdated() {
        int selectedItemId = mDirectoryAdapter.getSelectedItemId();
        if (selectedItemId == NO_SELECTED_ITEM_ID
                || mDialogType == DownloadLocationDialogType.LOCATION_FULL
                || mDialogType == DownloadLocationDialogType.LOCATION_NOT_FOUND) {
            selectedItemId = mDirectoryAdapter.useFirstValidSelectableItemId();
        }
        if (mDialogType == DownloadLocationDialogType.LOCATION_SUGGESTION) {
            selectedItemId = mDirectoryAdapter.useSuggestedItemId(mTotalBytes);
        }

        mFileLocation.setAdapter(mDirectoryAdapter);
        mFileLocation.setSelection(selectedItemId);

        // Show "not enough space" error text the new chosen storage doesn't have enough space.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SMART_SUGGESTION_FOR_LARGE_DOWNLOADS)) {
            mFileLocation.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(
                        AdapterView<?> parent, View view, int position, long id) {
                    DirectoryOption option = (DirectoryOption) mDirectoryAdapter.getItem(position);
                    setLocationAvailableSpace(option.availableSpace);
                }

                @Override
                public void onNothingSelected(AdapterView<?> parent) {
                    // No callback. Only update listeners when an actual option is selected.
                }
            });
        }
    }

    @Override
    public void onDirectorySelectionChanged() {}
}
