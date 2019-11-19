// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.download;

import android.os.Bundle;
import android.support.v7.preference.PreferenceDialogFragmentCompat;
import android.view.View;
import android.widget.ListView;

import org.chromium.chrome.download.R;

/**
 * The dialog used to display the download directory preference choices.
 */
public class DownloadLocationPreferenceDialog extends PreferenceDialogFragmentCompat {
    public static final String TAG = "DownloadLocationPreferenceDialog";

    public static DownloadLocationPreferenceDialog newInstance(
            DownloadLocationPreference preference) {
        DownloadLocationPreferenceDialog fragment = new DownloadLocationPreferenceDialog();
        Bundle bundle = new Bundle(1);
        bundle.putString(PreferenceDialogFragmentCompat.ARG_KEY, preference.getKey());
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    protected void onBindDialogView(View view) {
        DownloadLocationPreference preference = (DownloadLocationPreference) getPreference();
        ListView listView = view.findViewById(R.id.location_preference_list_view);
        listView.setAdapter(preference.getAdapter());
        super.onBindDialogView(view);
    }

    /**
     * Do nothing. Preferences are already updated at {@link
     * DownloadLocationPreferenceAdapter#onClick(View)}.
     */
    @Override
    public void onDialogClosed(boolean b) {}
}
