// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.settings;

import android.content.Context;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.ListView;

import androidx.preference.PreferenceDialogFragmentCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.R;

/** The dialog used to display the download directory preference choices. */
@NullMarked
public class DownloadLocationPreferenceDialog extends PreferenceDialogFragmentCompat {
    public static final String TAG = "DownloadLocationPreferenceDialog";

    private @Nullable Context mThemedContext;

    public static DownloadLocationPreferenceDialog newInstance(
            DownloadLocationPreference preference) {
        DownloadLocationPreferenceDialog fragment = new DownloadLocationPreferenceDialog();
        Bundle bundle = new Bundle(1);
        bundle.putString(PreferenceDialogFragmentCompat.ARG_KEY, preference.getKey());
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    public void onAttach(Context context) {
        // This dialog is hosted by the activity, not by the settings fragment, so it does not
        // inherit the theme applied by ChromeBaseSettingsFragment. With SettingsInTab the host is
        // ChromeTabbedActivity, whose theme does not set `alertDialogTheme`, so the dialog would
        // be drawn without rounded corners. Apply the settings theme overlay here so the dialog
        // looks the same regardless of which activity hosts settings.
        mThemedContext = new ContextThemeWrapper(context, R.style.ThemeOverlay_Chromium_Settings);
        super.onAttach(mThemedContext);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        // Avoid holding on to a context for a detached activity.
        mThemedContext = null;
    }

    @Override
    public @Nullable Context getContext() {
        return mThemedContext != null ? mThemedContext : super.getContext();
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
