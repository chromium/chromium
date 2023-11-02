// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.content.ClipboardManager;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.net.LinkProperties;
import android.os.Build;
import android.os.LocaleList;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextSelection;

import androidx.annotation.RequiresApi;

/**
 * Utility class to use new APIs that were added in P (API level 28). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@RequiresApi(Build.VERSION_CODES.P)
public final class ApiHelperForP {
    private ApiHelperForP() {}

    /** See {@link LinkProperties#isPrivateDnsActive() }. */
    public static boolean isPrivateDnsActive(LinkProperties linkProperties) {
        return linkProperties.isPrivateDnsActive();
    }

    /** See {@link LinkProperties#getPrivateDnsServerName() }. */
    public static String getPrivateDnsServerName(LinkProperties linkProperties) {
        return linkProperties.getPrivateDnsServerName();
    }

    /** See {@link PackageInfo#getLongVersionCode() }. */
    public static long getLongVersionCode(PackageInfo packageInfo) {
        return packageInfo.getLongVersionCode();
    }

    /** See {@link LocationManager#isLocationEnabled() }. */
    public static boolean isLocationEnabled(LocationManager locationManager) {
        return locationManager.isLocationEnabled();
    }

    /** See {@link TelephonyManager#getSignalStrength() }. */
    public static SignalStrength getSignalStrength(TelephonyManager telephonyManager) {
        return telephonyManager.getSignalStrength();
    }

    /** See {@link ClipboardManager#clearPrimaryClip() }. */
    public static void clearPrimaryClip(ClipboardManager clipboardManager) {
        clipboardManager.clearPrimaryClip();
    }

    /** See {@link PackageManager#hasSigningCertificate(String, byte[], int) }. */
    public static boolean hasSigningCertificate(
            PackageManager pm, String packageName, byte[] certificate, int type) {
        return pm.hasSigningCertificate(packageName, certificate, type);
    }

    /** See {@link TextClassifier#suggestSelection() } */
    public static TextSelection suggestSelection(
            TextClassifier textClassifier, TextSelection.Request request) {
        return textClassifier.suggestSelection(request);
    }

    /** See {@link TextSelection.Request.Builder#Builder() } */
    public static TextSelection.Request.Builder newTextSelectionRequestBuilder(
            CharSequence text, int startIndex, int endIndex) {
        return new TextSelection.Request.Builder(text, startIndex, endIndex);
    }

    /** See {@link TextSelection.Request.Builder#setDefaultLocales() } */
    public static TextSelection.Request.Builder setDefaultLocales(
            TextSelection.Request.Builder builder, LocaleList defaultLocales) {
        return builder.setDefaultLocales(defaultLocales);
    }

    /** See {@link TextSelection.Request.Builder#build() } */
    public static TextSelection.Request build(TextSelection.Request.Builder builder) {
        return builder.build();
    }
}
