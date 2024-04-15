// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.rlz;

import android.text.TextUtils;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.uid.SettingsSecureBasedIdentificationGenerator;

import java.util.List;
import java.util.Locale;

/** A handler for revenue related pings that needs customized brand and event codes. */
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
        String id = new SettingsSecureBasedIdentificationGenerator().getUniqueId(ID_SALT);
        id = generate50CharacterId(id.toUpperCase(Locale.getDefault()));

        RlzPingHandlerJni.get()
                .startPing(
                        ProfileManager.getLastUsedRegularProfile(),
                        brand,
                        Locale.getDefault().getLanguage(),
                        TextUtils.join(",", events),
                        id,
                        callback);
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
        void startPing(
                @JniType("Profile*") Profile profile,
                String brand,
                String language,
                String events,
                String id,
                Callback<Boolean> callback);
    }
}
