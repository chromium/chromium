// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.support.annotation.Nullable;
import android.text.SpannableString;
import android.text.style.ImageSpan;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsBridge;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsEnabledStateUtils;
import org.chromium.chrome.browser.contextual_suggestions.EnabledStateMonitor;
import org.chromium.chrome.browser.signin.AccountSigninActivity;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninActivity;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Fragment to manage the Contextual Suggestions preference and to explain to the user what it does.
 */
public class ContextualSuggestionsPreference
        extends PreferenceFragment implements EnabledStateMonitor.Observer {
    static final String PREF_CONTEXTUAL_SUGGESTIONS_SWITCH = "contextual_suggestions_switch";
    private static final String PREF_CONTEXTUAL_SUGGESTIONS_DESCRIPTION =
            "contextual_suggestions_description";
    private static final String PREF_CONTEXTUAL_SUGGESTIONS_MESSAGE =
            "contextual_suggestions_message";

    private ChromeSwitchPreference mSwitch;
    private EnabledStateMonitor mEnabledStateMonitor;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.contextual_suggestions_preferences);
        getActivity().setTitle(R.string.prefs_contextual_suggestions);

        mSwitch = (ChromeSwitchPreference) findPreference(PREF_CONTEXTUAL_SUGGESTIONS_SWITCH);
        mEnabledStateMonitor =
                ChromeApplication.getComponent().resolveContextualSuggestionsEnabledStateMonitor();
        mEnabledStateMonitor.addObserver(this);
        onSettingsStateChanged(mEnabledStateMonitor.getSettingsEnabled());
        initialize();
    }

    @Override
    public void onResume() {
        super.onResume();
        updateSwitch();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mEnabledStateMonitor.removeObserver(this);
    }

    @Override
    public void onEnabledStateChanged(boolean enabled) {}

    @Override
    public void onSettingsStateChanged(boolean enabled) {
        if (mEnabledStateMonitor != null) updateSwitch();
    }

    /** Helper method to initialize the switch preference and the message preference. */
    private void initialize() {
        Context context = getActivity();
        final TextMessagePreference message =
                (TextMessagePreference) findPreference(PREF_CONTEXTUAL_SUGGESTIONS_MESSAGE);

        // Show a message prompting the user to turn on required settings. If unified consent is
        // enabled, and the proper settings are already enabled, show nothing.
        boolean isUnifiedConsentEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.UNIFIED_CONSENT);
        boolean isSignedIn = ChromeSigninController.get().isSignedIn();
        if (!isUnifiedConsentEnabled || !isSignedIn
                || (!ProfileSyncService.get().isUrlKeyedDataCollectionEnabled(false)
                           && !ProfileSyncService.get().isUrlKeyedDataCollectionEnabled(true))) {
            final NoUnderlineClickableSpan span = new NoUnderlineClickableSpan((widget) -> {
                if (isUnifiedConsentEnabled) {
                    if (isSignedIn) {
                        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                                context, SyncAndServicesPreferences.class.getName());
                        IntentUtils.safeStartActivity(context, intent);
                    } else {
                        startActivity(SigninActivity.createIntentForPromoChooseAccountFlow(
                                context, SigninAccessPoint.SETTINGS, null));
                    }
                } else {
                    if (isSignedIn) {
                        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                                context, SyncCustomizationFragment.class.getName());
                        IntentUtils.safeStartActivity(context, intent);
                    } else {
                        startActivity(AccountSigninActivity.createIntentForDefaultSigninFlow(
                                context, SigninAccessPoint.SETTINGS, false));
                    }
                }
            });
            final SpannableString spannable = SpanApplier.applySpans(
                    getResources().getString(isUnifiedConsentEnabled
                                    ? R.string.contextual_suggestions_message_unified_consent
                                    : R.string.contextual_suggestions_message),
                    new SpanApplier.SpanInfo("<link>", "</link>", span));
            message.setTitle(spannable);
        }

        final TextMessagePreference description =
                (TextMessagePreference) findPreference(PREF_CONTEXTUAL_SUGGESTIONS_DESCRIPTION);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BUTTON)) {
            TintedDrawable drawable = TintedDrawable.constructTintedDrawable(
                    context, R.drawable.contextual_suggestions, R.color.default_icon_color);
            drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
            final ImageSpan imageSpan = new ImageSpan(drawable);
            final SpannableString imageSpannable = SpanApplier.applySpans(
                    getResources().getString(
                            R.string.contextual_suggestions_description_toolbar_button),
                    new SpanApplier.SpanInfo("<icon>", "</icon>", imageSpan));
            description.setTitle(imageSpannable);
        } else {
            description.setTitle(
                    getResources().getString(R.string.contextual_suggestions_description));
        }

        updateSwitch();
        mSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            boolean enabled = (boolean) newValue;
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.CONTEXTUAL_SUGGESTIONS_ENABLED, enabled);

            ContextualSuggestionsEnabledStateUtils.recordPreferenceEnabled(enabled);
            if (enabled) {
                RecordUserAction.record("ContextualSuggestions.Preference.Enabled");
            } else {
                RecordUserAction.record("ContextualSuggestions.Preference.Disabled");
            }
            return true;
        });
        mSwitch.setManagedPreferenceDelegate(
                preference -> ContextualSuggestionsBridge.isDisabledByEnterprisePolicy());
    }

    /** Helper method to update the enabled state of the switch. */
    private void updateSwitch() {
        mSwitch.setEnabled(mEnabledStateMonitor.getSettingsEnabled());
        mSwitch.setChecked(mEnabledStateMonitor.getEnabledState());
    }
}
