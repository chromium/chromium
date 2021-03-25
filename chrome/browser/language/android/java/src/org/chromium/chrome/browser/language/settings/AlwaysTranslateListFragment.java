// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.content.Context;

import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.translate.TranslateBridge;

import java.util.Collection;

/**
 * Chrome Fragment for the always translate that provides UI for adding and removing languages.
 * Should be launched from a {@link LanguageItemListPreference}.
 */
public class AlwaysTranslateListFragment extends LanguageItemListFragment {
    @Override
    protected LanguageItemListFragment.ListDelegate makeFragmentListDelegate() {
        return new ListDelegate();
    }

    @Override
    protected String getLanguageListTitle(Context context) {
        return context.getResources().getString(R.string.languages_settings_automatic_title);
    }

    @Override
    protected void onLanguageAdded(String code) {
        TranslateBridge.setLanguageAlwaysTranslateState(code, true);
    }

    @Override
    protected void onLanguageRemoved(String code) {
        TranslateBridge.setLanguageAlwaysTranslateState(code, false);
    }

    /**
     * Helper class to populate the LanguageItem list and used by {@link LanguageItemListPreference}
     * to make the summary text and launch an Intent to this Fragment.
     */
    public static class ListDelegate implements LanguageItemListFragment.ListDelegate {
        @Override
        public Collection<LanguageItem> getLanguageItems() {
            return LanguagesManager.getInstance().getAlwaysTranslateLanguageItems();
        }

        @Override
        public String getFragmentClassName() {
            return AlwaysTranslateListFragment.class.getName();
        }
    }
}
