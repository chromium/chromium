// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check.helper;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.password_check.CompromisedCredential;
import org.chromium.chrome.browser.password_check.PasswordChangeType;
import org.chromium.chrome.browser.password_check.PasswordCheckComponentUi;
import org.chromium.chrome.browser.password_check.PasswordCheckUkmRecorder;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.Objects;

/**
 * Helper to launch apps, settings screens, or Chrome Custom tabs that enable the user to change a
 * compromised password.
 */
public class PasswordCheckChangePasswordHelper {
    private static final String AUTOFILL_ASSISTANT_PACKAGE =
            "org.chromium.chrome.browser.autofill_assistant.";
    private static final String AUTOFILL_ASSISTANT_ENABLED_KEY =
            AUTOFILL_ASSISTANT_PACKAGE + "ENABLED";
    private static final String PASSWORD_CHANGE_USERNAME_PARAMETER = "PASSWORD_CHANGE_USERNAME";
    private static final String PASSWORD_CHANGE_SKIP_LOGIN_PARAMETER = "PASSWORD_CHANGE_SKIP_LOGIN";
    private static final String INTENT_PARAMETER = "INTENT";
    private static final String SOURCE_PARAMETER = "SOURCE";
    private static final String INTENT = "PASSWORD_CHANGE";
    private static final String START_IMMEDIATELY_PARAMETER = "START_IMMEDIATELY";
    private static final String ORIGINAL_DEEPLINK_PARAMETER = "ORIGINAL_DEEPLINK";
    private static final String CALLER_PARAMETER = "CALLER";

    private static final int IN_CHROME_CALLER = 7;
    private static final int SOURCE_PASSWORD_CHANGE_SETTINGS = 11;

    private static final String ENCODING = "UTF-8";

    private final Context mContext;
    private final SettingsLauncher mSettingsLauncher;
    private final PasswordCheckComponentUi.CustomTabIntentHelper mCustomTabIntentHelper;
    private final PasswordCheckComponentUi.TrustedIntentHelper mTrustedIntentHelper;

    public PasswordCheckChangePasswordHelper(Context context, SettingsLauncher settingsLauncher,
            PasswordCheckComponentUi.CustomTabIntentHelper customTabIntentHelper,
            PasswordCheckComponentUi.TrustedIntentHelper trustedIntentHelper) {
        mContext = context;
        mSettingsLauncher = settingsLauncher;
        mCustomTabIntentHelper = customTabIntentHelper;
        mTrustedIntentHelper = trustedIntentHelper;
    }

    /**
     * Launches an app (if available) or a CCT with the site the given credential was used on.
     * @param credential A {@link CompromisedCredential}.
     */
    public void launchAppOrCctWithChangePasswordUrl(CompromisedCredential credential) {
        if (!canManuallyChangeCredential(credential)) return;
        // TODO(crbug.com/1092444): Always launch the URL if possible and let Android handle the
        // match to open it.
        IntentUtils.safeStartActivity(mContext,
                credential.getAssociatedApp().isEmpty()
                        ? buildIntent(
                                credential.getPasswordChangeUrl(), PasswordChangeType.MANUAL_CHANGE)
                        : getPackageLaunchIntent(credential.getAssociatedApp()));
    }

    /**
     * @param credential A {@link CompromisedCredential}.
     * @return True iff there is a valid URL to navigate to or an app that can be opened.
     */
    public boolean canManuallyChangeCredential(CompromisedCredential credential) {
        return !credential.getPasswordChangeUrl().isEmpty()
                || getPackageLaunchIntent(credential.getAssociatedApp()) != null;
    }

    /**
     * Launches a CCT with the site the given credential was used on and invokes the script that
     * fixes the compromised credential automatically.
     *
     * The associated URL will always contain a valid URL, never an Android app sign-on realm
     * as scripts will only exist for websites.
     * @param credential A {@link CompromisedCredential}.
     */
    public void launchCctWithScript(CompromisedCredential credential) {
        String origin = credential.getAssociatedUrl().getOrigin().getSpec();
        Intent intent = buildIntent(origin, PasswordChangeType.AUTOMATED_CHANGE);
        populateAutofillAssistantExtras(intent, origin, credential.getUsername());
        IntentUtils.safeStartActivity(mContext, intent);
    }

    private Intent getPackageLaunchIntent(String packageName) {
        return Objects.requireNonNull(mContext).getPackageManager().getLaunchIntentForPackage(
                packageName);
    }

    /**
     * Builds an intent to launch a CCT.
     *
     * @param initialUrl Initial URL to launch a CCT.
     * @param passwordChangeType Password change type.
     * @return {@link Intent} for CCT.
     */
    private Intent buildIntent(String initialUrl, @PasswordChangeType int passwordChangeType) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(initialUrl));
        Intent intent = mCustomTabIntentHelper.createCustomTabActivityIntent(
                mContext, customTabIntent.intent);
        intent.setPackage(mContext.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        intent.putExtra(PasswordCheckUkmRecorder.PASSWORD_CHECK_PACKAGE
                        + PasswordCheckUkmRecorder.PASSWORD_CHANGE_TYPE,
                passwordChangeType);
        mTrustedIntentHelper.addTrustedIntentExtras(intent);
        return intent;
    }

    /**
     * Populates intent extras for an Autofill Assistant script.
     *
     * @param intent   An {@link Intent} to be populated.
     * @param origin   An origin for a password change script. One of extras to put.
     * @param username A username for a password change script. One of extras to put.
     */
    private void populateAutofillAssistantExtras(Intent intent, String origin, String username) {
        intent.putExtra(AUTOFILL_ASSISTANT_ENABLED_KEY, true);
        intent.putExtra(AUTOFILL_ASSISTANT_PACKAGE + INTENT_PARAMETER, INTENT);
        intent.putExtra(AUTOFILL_ASSISTANT_PACKAGE + START_IMMEDIATELY_PARAMETER, true);
        intent.putExtra(AUTOFILL_ASSISTANT_PACKAGE + CALLER_PARAMETER, IN_CHROME_CALLER);
        intent.putExtra(
                AUTOFILL_ASSISTANT_PACKAGE + SOURCE_PARAMETER, SOURCE_PASSWORD_CHANGE_SETTINGS);
        intent.putExtra(AUTOFILL_ASSISTANT_PACKAGE + PASSWORD_CHANGE_SKIP_LOGIN_PARAMETER, false);
        // Note: All string-typed parameters must be URL-encoded, because the
        // corresponding extraction logic will URL-*de*code them before use,
        // see TriggerContext.java.
        try {
            intent.putExtra(AUTOFILL_ASSISTANT_PACKAGE + ORIGINAL_DEEPLINK_PARAMETER,
                    URLEncoder.encode(origin, ENCODING));
            intent.putExtra(AUTOFILL_ASSISTANT_PACKAGE + PASSWORD_CHANGE_USERNAME_PARAMETER,
                    URLEncoder.encode(username, ENCODING));
        } catch (UnsupportedEncodingException e) {
            throw new IllegalStateException("Encoding not available.", e);
        }
    }
}
