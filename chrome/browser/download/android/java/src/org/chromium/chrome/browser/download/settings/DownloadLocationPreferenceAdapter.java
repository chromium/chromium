// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.settings;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.StringUtils;

/**
 * Class used to provide data shown in the download location preference in download settings page.
 */
public class DownloadLocationPreferenceAdapter extends DownloadDirectoryAdapter
        implements OnClickListener {
    /** Constructor of DownloadLocationPreferenceAdapter. */
    public DownloadLocationPreferenceAdapter(Context context, Delegate delegate) {
        super(context, delegate);
    }

    @Override
    public View getView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
        View view = convertView;
        if (view == null) {
            view =
                    LayoutInflater.from(getContext())
                            .inflate(R.layout.download_location_preference_item, null);
        }

        view.setTag(position);
        view.setOnClickListener(this);

        RadioButton radioButton = view.findViewById(R.id.radio_button);
        radioButton.setChecked(getSelectedItemId() == position);
        radioButton.setTag(position);
        radioButton.setOnClickListener(this);

        // Only show the radio button when there are multiple items.
        if (getCount() <= 1) radioButton.setVisibility(View.GONE);

        view.setEnabled(isEnabled(position));

        DirectoryOption directoryOption = (DirectoryOption) getItem(position);
        if (directoryOption == null) return view;

        TextView titleText = (TextView) view.findViewById(R.id.title);
        titleText.setText(directoryOption.name);

        TextView summaryText = (TextView) view.findViewById(R.id.description);
        if (isEnabled(position)) {
            String summary =
                    StringUtils.getAvailableBytesForUi(
                            getContext(), directoryOption.availableSpace);
            summaryText.setText(summary);

            // Build description for accessibility.
            StringBuilder accessibilityDescription = new StringBuilder();
            accessibilityDescription.append(directoryOption.name);
            accessibilityDescription.append(" ");
            accessibilityDescription.append(summary);
            radioButton.setContentDescription(accessibilityDescription);
        } else {
            radioButton.setEnabled(false);
            titleText.setEnabled(false);
            summaryText.setEnabled(false);

            if (hasAvailableLocations()) {
                summaryText.setText(
                        getContext().getText(R.string.download_location_not_enough_space));
            } else {
                summaryText.setVisibility(View.GONE);
            }
        }

        return view;
    }

    @Override
    public void onClick(View v) {
        int selectedId = (int) v.getTag();
        DirectoryOption option = (DirectoryOption) getItem(selectedId);
        if (option == null) return;

        // Update the native pref, which persists the download directory selected by the user.
        mDelegate
                .getDownloadLocationHelper()
                .setDownloadAndSaveFileDefaultDirectory(option.location);

        mSelectedPosition = selectedId;

        // Update the preference after selected position is updated.
        mDelegate.onDirectorySelectionChanged();

        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.Location.Setting.DirectoryType",
                option.type,
                DirectoryOption.DownloadLocationDirectoryType.NUM_ENTRIES);

        // Refresh the list of download directories UI.
        notifyDataSetChanged();
    }
}
