// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

import java.util.ArrayList;

/**
 * Chrome Preference that is used to launch a {@link LanguageItemListFragment}. The preference
 * summary is updated to refelect the first elements of the list.
 */
public class LanguageItemListPreference extends ChromeBasePreference
        implements PrefChangeRegistrar.PrefObserver {
    // Default number of items to list in a collection preference summary.
    private static final int COLLECTION_SUMMARY_ITEM_LIMIT = 3;

    private LanguageItemListFragment.ListDelegate mLanguageItemListDelegate;

    public LanguageItemListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setFragmentListDelegate(LanguageItemListFragment.ListDelegate listDelegate) {
        mLanguageItemListDelegate = listDelegate;
        updateSummary();
    }

    /**
     * @return The class of the Fragment to launch when this preference is clicked.
     */
    public Class<? extends Fragment> getFragmentClass() {
        return mLanguageItemListDelegate.getFragmentClass();
    }

    /**
     * If the LanguageItemListDelegate for this preference is set update the summary. Otherwise
     * leave the summary as is.
     */
    public void updateSummary() {
        String summary = makeSummary();
        if (summary == null) return;
        setSummary(summary);
    }

    /**
     * Allows this preference to be registared as a preference observer and update the summary when
     * the preference changes. From {@link PrefChangeRegistrar.PrefObserver}.
     */
    @Override
    public void onPreferenceChange() {
        updateSummary();
    }

    /**
     * If the ListDelegate for this preference is set return a comma separated string of
     * display names for at most the first three languages. If the delegate is not set return null.
     * @param languages List of LanguageItems.
     * @return Comma sepperated string of language display names.
     */
    private String makeSummary() {
        if (mLanguageItemListDelegate == null) return null;
        int index = 0;
        ArrayList<String> languageNames = new ArrayList<String>();
        for (LanguageItem item : mLanguageItemListDelegate.getLanguageItems()) {
            if (++index > COLLECTION_SUMMARY_ITEM_LIMIT) break;
            languageNames.add(item.getDisplayName());
        }
        // TODO(crbug.com/40170296): Make sure to localize the separator.
        return TextUtils.join(", ", languageNames);
    }
}
