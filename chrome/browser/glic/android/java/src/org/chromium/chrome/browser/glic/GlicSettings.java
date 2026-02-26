// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Fragment for GLIC related configurations to Chrome. */
@NullMarked
public class GlicSettings extends ChromeBaseSettingsFragment {
    private static final String PERMISSION_DEFAULT_TAB_ACCESS =
            "glic_permissions_default_tab_access";
    private static final String LEARN_MORE_AI_URL = "https://support.google.com/a/answer/15706919";

    public static final String PREF_KEY_GLIC_PERMISSIONS_ACTIVITY = "glic_permissions_activity";
    public static final String PREF_KEY_GLIC_EXTENSIONS = "glic_extensions";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.glic_settings);
        mPageTitle.set(getString(R.string.settings_glic_button_toggle));

        ChromeSwitchPreference tabAccessPref =
                assertNonNull(findPreference(PERMISSION_DEFAULT_TAB_ACCESS));
        var summary =
                getString(
                        R.string
                                .settings_glic_permissions_default_tab_access_toggle_sublabel_data_protected);
        tabAccessPref.setSummary(
                SpanApplier.applySpans(summary, getLearnMoreSpanInfo(LEARN_MORE_AI_URL)));

        Preference permissionActivityPref =
                assertNonNull(findPreference(PREF_KEY_GLIC_PERMISSIONS_ACTIVITY));
        permissionActivityPref.setOnPreferenceClickListener(
                preference -> {
                    String url = getString(R.string.settings_glic_permissions_activity_button_url);
                    getCustomTabLauncher().openUrlInCct(getActivity(), url);
                    return true;
                });

        Preference permissionConnectedAppsPref = findPreference(PREF_KEY_GLIC_EXTENSIONS);
        if (permissionConnectedAppsPref != null) {
            permissionConnectedAppsPref.setOnPreferenceClickListener(
                    preference -> {
                        String url = getString(R.string.settings_glic_extensions_button_url);
                        getCustomTabLauncher().openUrlInCct(getActivity(), url);
                        return true;
                    });
        }
    }

    private SpanApplier.SpanInfo getLearnMoreSpanInfo(String url) {
        return new SpanApplier.SpanInfo(
                "<a href=\"#\">",
                "</a>",
                new ChromeClickableSpan(
                        getContext(),
                        v -> {
                            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                            IntentUtils.safeStartActivity(getContext(), intent);
                        }));
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    // TODO(crbug.com/480218604): Override #updateDynamicPreferences once the preference is
    // implemented.

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(GlicSettings.class.getName(), R.xml.glic_settings);
}
