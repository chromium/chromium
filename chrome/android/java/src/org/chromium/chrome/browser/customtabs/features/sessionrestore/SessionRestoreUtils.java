// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.StringDef;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Utils functions for session restore.
 */
public class SessionRestoreUtils {
    private SessionRestoreUtils() {}

    /**
     * Identifier used for last CCT client App. Used to pass suffix for histogram
     * in CustomTabActivityLifecycleUmaTracker#recordForRetainableSessions.
     */
    @StringDef({ClientIdentifierType.DIFFERENT, ClientIdentifierType.MIXED,
            ClientIdentifierType.REFERRER, ClientIdentifierType.PACKAGE_NAME})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClientIdentifierType {
        String DIFFERENT = ".Different";
        String MIXED = ".Mixed";
        String REFERRER = ".Referrer";
        String PACKAGE_NAME = ".PackageName";
    }

    /**
     * Determine if the current session is restorable by comparing its parameters to the previous
     * session. Delegate to #getClientIdentifier type for the actual comparison once the basic
     * criteria that we have a package name or referrer, we launch with the same URL, and the
     * previous session had user interaction are established.
     *
     * @param taskId The task Id of CCT activity.
     * @param url The URL the CCT is launched with.
     * @param preferences Instance from {@link SharedPreferencesManager#getInstance()}.
     * @param clientPackage Package name get from CCT service.
     * @param referrer Referrer of the CCT activity.
     */
    public static boolean canRestoreSession(int taskId, String url,
            SharedPreferencesManager preferences, @Nullable String clientPackage,
            @Nullable String referrer) {
        boolean hasPackageName = !TextUtils.isEmpty(clientPackage);
        boolean hasReferrer = !TextUtils.isEmpty(referrer);

        if (!hasPackageName && !hasReferrer) {
            return false;
        }

        boolean launchWithSameUrl = TextUtils.equals(
                url, preferences.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL, ""));
        if (!launchWithSameUrl) {
            return false;
        }

        boolean prevSessionRetainable = preferences.readBoolean(
                ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, false);
        if (!prevSessionRetainable) {
            return false;
        }

        String prevClientPackage =
                preferences.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE, null);
        String prevReferrer =
                preferences.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER, null);
        int prevTaskId = preferences.readInt(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID);

        return !getClientIdentifierType(
                clientPackage, prevClientPackage, referrer, prevReferrer, taskId, prevTaskId)
                        .equals(ClientIdentifierType.DIFFERENT);
    }

    /**
     * Returns the type of Custom Tab session being launched with regards to if it can be restored.
     * All sessions with ClientIdentifierType != 'DIFFERENT' are restorable. The embedded app is
     * determined through taskId + package name combination. For the package name to use, this
     * function will bias clientPackage if provided, otherwise fallback to referrer.
     *
     * @param clientPackage the client package CCT is currently launched from, if it can be known.
     * @param prevClientPackage the client package the last CCT was launched from.
     * @param referrer the referrer for the current CCT activity, if it can be known.
     * @param prevReferrer the referrer for the last CCT activity, if one exists.
     * @param taskId the taskId of the current CCT activity.
     * @param prevTaskId taskId for the previous CCT activity, if one exists.
     * @return ClientIdentifier for the CCT client app.
     */
    public static String getClientIdentifierType(String clientPackage, String prevClientPackage,
            String referrer, String prevReferrer, int taskId, int prevTaskId) {
        boolean hasClientPackage = !TextUtils.isEmpty(clientPackage);
        boolean hasReferrer = !TextUtils.isEmpty(referrer);
        String clientIdType = ClientIdentifierType.DIFFERENT;
        if (hasClientPackage && TextUtils.equals(clientPackage, prevClientPackage)) {
            clientIdType = ClientIdentifierType.PACKAGE_NAME;
        } else if (hasReferrer && TextUtils.equals(referrer, prevReferrer)
                && prevTaskId == taskId) {
            clientIdType = ClientIdentifierType.REFERRER;
        } else if (hasClientPackage || prevTaskId == taskId) {
            String currentPackage =
                    hasClientPackage ? clientPackage : Uri.parse(referrer).getHost();
            String prevPackage = !TextUtils.isEmpty(prevClientPackage)
                    ? prevClientPackage
                    : Uri.parse(prevReferrer).getHost();

            if (TextUtils.equals(currentPackage, prevPackage)
                    && !TextUtils.isEmpty(currentPackage)) {
                clientIdType = ClientIdentifierType.MIXED;
            }
        }
        return clientIdType;
    }
}
