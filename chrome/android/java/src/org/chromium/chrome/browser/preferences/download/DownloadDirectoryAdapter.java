// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.download;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.AppCompatImageView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.download.R;

import java.util.ArrayList;
import java.util.List;

/**
 * Custom adapter that populates the list of which directories the user can choose as their default
 * download location.
 */
public class DownloadDirectoryAdapter extends ArrayAdapter<Object> {
    /**
     * Delegate to handle directory options results and observe data changes.
     */
    public interface Delegate {
        /**
         * Called when available download directories are changed, like SD card removal. App level
         * UI logic should update to match the new backend data.
         */
        void onDirectoryOptionsUpdated();

        /**
         * Called after the user selected another download directory option.
         */
        void onDirectorySelectionChanged();
    }

    public static int NO_SELECTED_ITEM_ID = -1;
    public static int SELECTED_ITEM_NOT_INITIALIZED = -2;

    protected int mSelectedPosition = SELECTED_ITEM_NOT_INITIALIZED;

    private Context mContext;
    private LayoutInflater mLayoutInflater;
    protected Delegate mDelegate;

    private List<DirectoryOption> mCanonicalOptions = new ArrayList<>();
    private List<DirectoryOption> mAdditionalOptions = new ArrayList<>();
    private List<DirectoryOption> mErrorOptions = new ArrayList<>();

    public DownloadDirectoryAdapter(@NonNull Context context, Delegate delegate) {
        super(context, android.R.layout.simple_spinner_item);

        mContext = context;
        mDelegate = delegate;
        mLayoutInflater = LayoutInflater.from(context);
    }

    @Override
    public int getCount() {
        return mCanonicalOptions.size() + mAdditionalOptions.size() + mErrorOptions.size();
    }

    @Nullable
    @Override
    public Object getItem(int position) {
        if (!mErrorOptions.isEmpty()) {
            assert position == 0;
            assert getCount() == 1;
            return mErrorOptions.get(position);
        }

        return position < mCanonicalOptions.size()
                ? mCanonicalOptions.get(position)
                : mAdditionalOptions.get(position - mCanonicalOptions.size());
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @NonNull
    @Override
    public View getView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
        View view = convertView != null
                ? convertView
                : mLayoutInflater.inflate(R.layout.download_location_spinner_item, null);

        view.setTag(position);

        DirectoryOption directoryOption = (DirectoryOption) getItem(position);
        if (directoryOption == null) return view;

        TextView titleText = (TextView) view.findViewById(R.id.text);
        titleText.setText(directoryOption.name);

        // ModalDialogView may do a measure pass on the view hierarchy to limit the layout inside
        // certain area, where LayoutParams cannot be null.
        if (view.getLayoutParams() == null) {
            view.setLayoutParams(
                    new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }
        return view;
    }

    @Override
    public View getDropDownView(
            int position, @Nullable View convertView, @NonNull ViewGroup parent) {
        View view = convertView != null
                ? convertView
                : mLayoutInflater.inflate(R.layout.download_location_spinner_dropdown_item, null);

        view.setTag(position);

        DirectoryOption directoryOption = (DirectoryOption) getItem(position);
        if (directoryOption == null) return view;

        TextView titleText = (TextView) view.findViewById(R.id.title);
        TextView summaryText = (TextView) view.findViewById(R.id.description);
        boolean enabled = isEnabled(position);

        titleText.setText(directoryOption.name);
        titleText.setEnabled(enabled);
        summaryText.setEnabled(enabled);
        if (enabled) {
            summaryText.setText(DownloadUtils.getStringForAvailableBytes(
                    mContext, directoryOption.availableSpace));
        } else {
            if (mErrorOptions.isEmpty()) {
                summaryText.setText(mContext.getText(R.string.download_location_not_enough_space));
            } else {
                summaryText.setVisibility(View.GONE);
            }
        }

        AppCompatImageView imageView = (AppCompatImageView) view.findViewById(R.id.icon_view);
        imageView.setVisibility(View.GONE);

        return view;
    }

    @Override
    public boolean isEnabled(int position) {
        DirectoryOption directoryOption = (DirectoryOption) getItem(position);
        return directoryOption != null && directoryOption.availableSpace != 0;
    }

    /**
     * @return  ID of the directory option that matches the default download location.
     */
    public int getSelectedItemId() {
        return mSelectedPosition;
    }

    private void initSelectedIdFromPref() {
        if (!mErrorOptions.isEmpty()) return;

        int selectedId = NO_SELECTED_ITEM_ID;

        String defaultLocation = PrefServiceBridge.getInstance().getDownloadDefaultDirectory();
        for (int i = 0; i < getCount(); i++) {
            DirectoryOption option = (DirectoryOption) getItem(i);
            if (option == null) continue;
            if (defaultLocation.equals(option.location)) {
                selectedId = i;
                break;
            }
        }
        mSelectedPosition = selectedId;
    }

    /**
     * In the case that there is no selected item ID/the selected item ID is invalid (ie. there is
     * not enough space), select either the default or the next valid item ID. Set the default to be
     * this item and return the ID.
     *
     * @return  ID of the first valid, selectable item and the new default location.
     */
    public int useFirstValidSelectableItemId() {
        for (int i = 0; i < getCount(); i++) {
            DirectoryOption option = (DirectoryOption) getItem(i);
            if (option == null) continue;
            if (option.availableSpace > 0) {
                PrefServiceBridge.getInstance().setDownloadAndSaveFileDefaultDirectory(
                        option.location);
                mSelectedPosition = i;
                return i;
            }
        }

        // Display an option that says there are no available download locations.
        adjustErrorDirectoryOption();
        return 0;
    }

    boolean hasAvailableLocations() {
        return mErrorOptions.isEmpty();
    }

    /**
     * Update the list of items.
     */
    public void update() {
        mCanonicalOptions.clear();
        mAdditionalOptions.clear();
        mErrorOptions.clear();

        // Retrieve all download directories.
        DownloadDirectoryProvider.getInstance().getAllDirectoriesOptions(
                (ArrayList<DirectoryOption> dirs) -> { onDirectoryOptionsRetrieved(dirs); });
    }

    private void onDirectoryOptionsRetrieved(ArrayList<DirectoryOption> dirs) {
        int numOtherAdditionalDirectories = 0;
        for (DirectoryOption dir : dirs) {
            DirectoryOption directory = (DirectoryOption) dir.clone();
            switch (directory.type) {
                case DirectoryOption.DownloadLocationDirectoryType.DEFAULT:
                    directory.name = mContext.getString(R.string.menu_downloads);
                    mCanonicalOptions.add(directory);
                    break;
                case DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL:
                    String directoryName = (numOtherAdditionalDirectories > 0)
                            ? mContext.getString(org.chromium.chrome.R.string
                                                         .downloads_location_sd_card_number,
                                      numOtherAdditionalDirectories + 1)
                            : mContext.getString(
                                      org.chromium.chrome.R.string.downloads_location_sd_card);
                    directory.name = directoryName;
                    mAdditionalOptions.add(directory);
                    numOtherAdditionalDirectories++;
                    break;
                case DirectoryOption.DownloadLocationDirectoryType.ERROR:
                    directory.name =
                            mContext.getString(R.string.download_location_no_available_locations);
                    mErrorOptions.add(directory);
                    break;
                default:
                    break;
            }
        }

        // Setup the selection.
        initSelectedIdFromPref();

        // Update lower Android level UI widgets.
        notifyDataSetChanged();

        // Update higher app level UI logic.
        if (mDelegate != null) mDelegate.onDirectoryOptionsUpdated();
    }

    private void adjustErrorDirectoryOption() {
        if ((mCanonicalOptions.size() + mAdditionalOptions.size()) > 0) {
            mErrorOptions.clear();
        } else {
            mErrorOptions.add(new DirectoryOption(
                    mContext.getString(R.string.download_location_no_available_locations), null, 0,
                    0, DirectoryOption.DownloadLocationDirectoryType.ERROR));
        }
    }
}
