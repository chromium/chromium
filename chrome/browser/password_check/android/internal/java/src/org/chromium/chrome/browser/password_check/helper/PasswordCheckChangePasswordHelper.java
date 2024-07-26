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

import java.util.Objects;

/**
 * Helper to launch apps, settings screens, or Chrome Custom tabs that enable the user to change a
 * compromised password.
 */
public class PasswordCheckChangePasswordHelper {
    private final Context mContext;
    private final PasswordCheckComponentUi.CustomTabIntentHelper mCustomTabIntentHelper;
    private final PasswordCheckComponentUi.TrustedIntentHelper mTrustedIntentHelper;

    public PasswordCheckChangePasswordHelper(
            Context context,
            PasswordCheckComponentUi.CustomTabIntentHelper customTabIntentHelper,
            PasswordCheckComponentUi.TrustedIntentHelper trustedIntentHelper) {
        mContext = context;
        mCustomTabIntentHelper = customTabIntentHelper;
        mTrustedIntentHelper = trustedIntentHelper;
    }

    /**
     * Launches an app (if available) or a CCT with the site the given credential was used on.
     *
     * @param credential A {@link CompromisedCredential}.
     */
    public void launchAppOrCctWithChangePasswordUrl(CompromisedCredential credential) {
        if (!canManuallyChangeCredential(credential)) return;
        // TODO(crbug.com/40134591): Always launch the URL if possible and let Android handle the
        // match to open it.
        IntentUtils.safeStartActivity(
                mContext,
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

    private Intent getPackageLaunchIntent(String packageName) {
        return Objects.requireNonNull(mContext)
                .getPackageManager()
                .getLaunchIntentForPackage(packageName);
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
        Intent intent =
                mCustomTabIntentHelper.createCustomTabActivityIntent(
                        mContext, customTabIntent.intent);
        intent.setPackage(mContext.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        intent.putExtra(
                PasswordCheckUkmRecorder.PASSWORD_CHECK_PACKAGE
                        + PasswordCheckUkmRecorder.PASSWORD_CHANGE_TYPE,
                passwordChangeType);
        mTrustedIntentHelper.addTrustedIntentExtras(intent);
        return intent;
    }
}
