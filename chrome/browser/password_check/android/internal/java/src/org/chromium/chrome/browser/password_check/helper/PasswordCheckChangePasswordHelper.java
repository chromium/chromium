// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check.helper;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.password_check.CompromisedCredential;
import org.chromium.chrome.browser.password_check.PasswordCheckEditFragmentView;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;

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
    private static final String INTENT_PARAMETER = "INTENT";
    private static final String INTENT = "PASSWORD_CHANGE";

    private final Context mContext;

    public PasswordCheckChangePasswordHelper(Context context) {
        mContext = context;
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
                        ? buildIntent(credential.getPasswordChangeUrl())
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
     * @param credential A {@link CompromisedCredential}.
     */
    public void launchCctWithScript(CompromisedCredential credential) {
        Intent intent = buildIntent(credential.getOrigin().getOrigin().getSpec());
        populateAutofillAssistantExtras(intent, credential.getUsername());
        IntentUtils.safeStartActivity(mContext, intent);
    }

    /**
     * Launches a settings fragment to edit the given credential.
     * @param credential A {@link CompromisedCredential} to change.
     */
    public void launchEditPage(CompromisedCredential credential) {
        SettingsLauncher launcher = new SettingsLauncherImpl();
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putParcelable(
                PasswordCheckEditFragmentView.EXTRA_COMPROMISED_CREDENTIAL, credential);
        launcher.launchSettingsActivity(
                mContext, PasswordCheckEditFragmentView.class, fragmentArgs);
    }

    private Intent getPackageLaunchIntent(String packageName) {
        return Objects.requireNonNull(mContext).getPackageManager().getLaunchIntentForPackage(
                packageName);
    }

    /**
     * Builds an intent to launch a CCT.
     *
     * @param initialUrl Initial URL to launch a CCT.
     * @return {@link Intent} for CCT.
     */
    private Intent buildIntent(String initialUrl) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(initialUrl));
        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                mContext, customTabIntent.intent);
        intent.setPackage(mContext.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        IntentHandler.addTrustedIntentExtras(intent);
        return intent;
    }

    /**
     * Populates intent extras for an Autofill Assistant script.
     *
     * @param intent   An {@link Intent} to be populated.
     * @param username A username for a password change script. One of extras to put.
     */
    private void populateAutofillAssistantExtras(Intent intent, String username) {
        intent.putExtra(AUTOFILL_ASSISTANT_ENABLED_KEY, true);
        intent.putExtra(AUTOFILL_ASSISTANT_PACKAGE + PASSWORD_CHANGE_USERNAME_PARAMETER, username);
        intent.putExtra(AUTOFILL_ASSISTANT_PACKAGE + INTENT_PARAMETER, INTENT);
        // TODO(crbug.com/1086114): Also add the following parameters when server side changes is
        // ready: CALLER, SOURCE. That would be useful for metrics.
    }
}