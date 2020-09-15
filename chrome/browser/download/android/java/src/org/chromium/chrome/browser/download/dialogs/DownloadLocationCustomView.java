// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.chromium.chrome.browser.download.settings.DownloadDirectoryAdapter.NO_SELECTED_ITEM_ID;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadLocationDialogType;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.settings.DownloadDirectoryAdapter;
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
    private Spinner mFileLocation;
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
        mFileLocation = findViewById(R.id.file_location);
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
                    // Hide the subtitle and adjust the bottom margin.
                    mSubtitleView.setVisibility(View.GONE);
                    MarginLayoutParams titleMargin = (MarginLayoutParams) mTitle.getLayoutParams();
                    titleMargin.bottomMargin = getResources().getDimensionPixelSize(
                            R.dimen.download_dialog_subtitle_margin_bottom);
                    setLayoutParams(titleMargin);
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
                // TODO(vuhung): Add download and storage info to subtitle.
                // Right now this subtitle is just a placeholder.
                // Putting name too long subtitle here to differentiate with default dialog.
                mSubtitleView.setText(R.string.download_location_name_too_long);
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
    }

    @Override
    public void onDirectorySelectionChanged() {}
}
