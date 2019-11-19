// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.rlz;

import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.identity.SettingsSecureBasedIdentificationGenerator;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.List;
import java.util.Locale;

/**
 * A handler for revenue related pings that needs customized brand and event codes.
 */
@JNINamespace("chrome::android")
public class RlzPingHandler {
    private static final String ID_SALT = "RLZSalt";

    private RlzPingHandler() {}

    /**
     * Generates a network ping with multiple events and a custom brand code. The application id is
     * always "chrome" and the language uses the default system language. The machine id is
     * a 50 character long generated string through
     * {@link SettingsSecureBasedIdentificationGenerator}.
     * @param brand The custom brand to be used for the ping.
     * @param events The list of events that should be sent with the ping.
     * @param callback A callback to be notified of the validity of the response received.
     */
    public static void startPing(
            String brand, List<String> events, final Callback<Boolean> callback) {
        String id =
                new SettingsSecureBasedIdentificationGenerator(ContextUtils.getApplicationContext())
                        .getUniqueId(ID_SALT);
        id = generate50CharacterId(id.toUpperCase(Locale.getDefault()));

        RlzPingHandlerJni.get().startPing(Profile.getLastUsedProfile().getOriginalProfile(), brand,
                Locale.getDefault().getLanguage(), TextUtils.join(",", events), id, callback);
    }

    private static String generate50CharacterId(String baseId) {
        StringBuilder idBuilder = new StringBuilder();
        while (idBuilder.length() < 50) {
            idBuilder.append(baseId);
        }
        return idBuilder.substring(0, 50);
    }

    @NativeMethods
    interface Natives {
        void startPing(Profile profile, String brand, String language, String events, String id,
                Callback<Boolean> callback);
    }
}
