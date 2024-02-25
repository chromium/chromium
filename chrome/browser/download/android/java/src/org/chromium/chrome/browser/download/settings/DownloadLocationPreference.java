// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.settings;

import android.content.Context;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.util.AttributeSet;

import androidx.preference.DialogPreference;

import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.settings.DownloadDirectoryAdapter.DownloadLocationHelper;

/** The preference used to save the download directory in download settings page. */
public class DownloadLocationPreference extends DialogPreference
        implements DownloadDirectoryAdapter.Delegate {
    /**
     * Provides data for the list of available download directories options. Uses an asynchronous
     * operation to query the directory options.
     */
    private DownloadLocationPreferenceAdapter mAdapter;

    private DownloadLocationHelper mLocationHelper;

    /** Constructor for DownloadLocationPreference. */
    public DownloadLocationPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setDialogLayoutResource(R.layout.download_location_preference);
        mAdapter = new DownloadLocationPreferenceAdapter(getContext(), this);
        mAdapter.update();
    }

    /** Set the helper to access and update the default download location. */
    public void setDownloadLocationHelper(DownloadLocationHelper helper) {
        mLocationHelper = helper;
    }

    /** Updates the summary that shows the download location directory. */
    public void updateSummary() {
        if (mAdapter.getSelectedItemId() < 0) return;

        DirectoryOption directoryOption =
                (DirectoryOption) mAdapter.getItem(mAdapter.getSelectedItemId());
        final SpannableStringBuilder summaryBuilder = new SpannableStringBuilder();
        summaryBuilder.append(directoryOption.name);
        summaryBuilder.append(" ");
        summaryBuilder.append(directoryOption.location);
        summaryBuilder.setSpan(
                new StyleSpan(android.graphics.Typeface.BOLD),
                0,
                directoryOption.name.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        setSummary(summaryBuilder);
    }

    // DownloadDirectoryAdapter.Delegate implementation.
    @Override
    public void onDirectoryOptionsUpdated() {
        if (mAdapter.getSelectedItemId() == DownloadDirectoryAdapter.NO_SELECTED_ITEM_ID) {
            mAdapter.useFirstValidSelectableItemId();
        }

        updateSummary();
    }

    @Override
    public void onDirectorySelectionChanged() {
        updateSummary();
    }

    @Override
    public DownloadLocationHelper getDownloadLocationHelper() {
        return mLocationHelper;
    }

    DownloadLocationPreferenceAdapter getAdapter() {
        return mAdapter;
    }
}
