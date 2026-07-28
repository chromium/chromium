// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.settings.AutofillAiDelegate.ToggleConfig;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
import org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextFragment;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.Set;

/** Fragment to manage Autofill AI Shopping. */
@NullMarked
public class AutofillShoppingFragment extends AutofillAiBaseFragment {

    public static final String PREF_OPT_IN_TOGGLE = "autofill_ai_shopping_opt_in";

    private static final ToggleConfig TOGGLE_CONFIG_SHOPPING =
            new ToggleConfig(
                    PREF_OPT_IN_TOGGLE,
                    R.string.autofill_shopping_opt_in_toggle_label,
                    R.string.autofill_shopping_opt_in_toggle_sub_label,
                    Pref.AUTOFILL_AI_SHOPPING_ENTITIES_ENABLED,
                    /* isPersonalContextSupported= */ true,
                    AutofillPersonalContextFragment.ACTION_ENTRY_FROM_SHOPPING);

    private static final Set<Integer> SHOPPING_TYPES =
            Set.of(EntityTypeName.ORDER, EntityTypeName.SHIPMENT);

    @Override
    protected AutofillAiDelegate createAutofillAiDelegate() {
        return new AutofillAiDelegate(this, this, TOGGLE_CONFIG_SHOPPING);
    }

    @Override
    protected int getTitleResId() {
        return R.string.autofill_shopping_title;
    }

    @Override
    protected Set<Integer> getEntityTypes() {
        return SHOPPING_TYPES;
    }

    @Override
    protected @AutofillOptionsReferrer int getReferrer() {
        return AutofillOptionsReferrer.AUTOFILL_SHOPPING_FRAGMENT;
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(AutofillShoppingFragment.class.getName(), 0) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    AutofillAiBaseFragment.updateDynamicPreferences(
                            this, indexData, profile, TOGGLE_CONFIG_SHOPPING);
                }
            };
}
