// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.content.Context;

import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.translate.TranslateBridge;

import java.util.Collection;

/**
 * Chrome Preference for the never translate list that displays the first three never translate
 * languages in the summary. Provides callbacks for adding and removing languages from the never
 * translate list in LanguageItemListFragment.
 */
public class NeverTranslateListFragment extends LanguageItemListFragment {
    @Override
    protected LanguageItemListFragment.ListDelegate makeFragmentListDelegate() {
        return new ListDelegate(getProfile());
    }

    @Override
    protected String getLanguageListTitle(Context context) {
        return context.getResources().getString(R.string.languages_settings_never_langs_title);
    }

    @Override
    protected @LanguagesManager.LanguageListType int getPotentialLanguageType() {
        return LanguagesManager.LanguageListType.NEVER_LANGUAGES;
    }

    @Override
    protected void recordFragmentImpression() {
        LanguagesManager.recordImpression(
                LanguagesManager.LanguageSettingsPageType.VIEW_NEVER_TRANSLATE_LANGUAGES);
    }

    @Override
    protected void recordAddLanguageImpression() {
        LanguagesManager.recordImpression(
                LanguagesManager.LanguageSettingsPageType.NEVER_TRANSLATE_ADD_LANGUAGE);
    }

    @Override
    protected void recordAddAction() {
        LanguagesManager.recordAction(
                LanguagesManager.LanguageSettingsActionType.ADD_TO_NEVER_TRANSLATE);
    }

    @Override
    protected void recordRemoveAction() {
        LanguagesManager.recordAction(
                LanguagesManager.LanguageSettingsActionType.REMOVE_FROM_NEVER_TRANSLATE);
    }

    @Override
    protected void onLanguageAdded(String code) {
        TranslateBridge.setLanguageBlockedState(getProfile(), code, true);
    }

    @Override
    protected void onLanguageRemoved(String code) {
        TranslateBridge.setLanguageBlockedState(getProfile(), code, false);
    }

    /**
     * Helper class to populate the LanguageItem list and used by {@link LanguageItemListPreference}
     * to make the summary text and launch an Intent to this Fragment.
     */
    public static class ListDelegate implements LanguageItemListFragment.ListDelegate {
        private final Profile mProfile;

        public ListDelegate(Profile profile) {
            mProfile = profile;
        }

        @Override
        public Collection<LanguageItem> getLanguageItems() {
            return LanguagesManager.getForProfile(mProfile).getNeverTranslateLanguageItems();
        }

        @Override
        public Class<NeverTranslateListFragment> getFragmentClass() {
            return NeverTranslateListFragment.class;
        }
    }
}
