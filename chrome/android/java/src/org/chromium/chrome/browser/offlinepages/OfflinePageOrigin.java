// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.Signature;
import android.os.Process;
import android.text.TextUtils;
import android.util.Base64;

import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

/**
 * Class encapsulating the application origin of a particular offline page request.
 */
public class OfflinePageOrigin {
    private final String mAppName;
    private final String[] mSignatures;

    /** Creates origin based on the context and tab. */
    public OfflinePageOrigin(Context context, Tab tab) {
        this(context, TabAssociatedApp.getAppId(tab));
    }

    /** Creates origin based on the context and an app name. */
    public OfflinePageOrigin(Context context, String appName) {
        if (TextUtils.isEmpty(appName)) {
            mAppName = "";
            mSignatures = null;
        } else {
            mSignatures = getAppSignaturesFor(context, appName);
            // If signatures returned null, the app probably doesn't exist. Assume Chrome.
            if (mSignatures == null) {
                mAppName = "";
            } else {
                mAppName = appName;
            }
        }
    }

    /** Creates origin based on a qualified string. Assumes Chrome if invalid. */
    public OfflinePageOrigin(String jsonString) {
        String name = "";
        String[] signatures = null;
        try {
            JSONArray info = new JSONArray(jsonString);
            if (info.length() == 2) {
                name = info.getString(0);
                JSONArray signatureInfo = info.getJSONArray(1);
                signatures = new String[signatureInfo.length()];
                for (int i = 0; i < signatures.length; i++) {
                    signatures[i] = signatureInfo.getString(i);
                }
            }
        } catch (JSONException e) {
            // JSON malformed. Set name and signature to default.
            name = "";
            signatures = null;
        } finally {
            mAppName = name;
            mSignatures = signatures;
        }
    }

    /** Creates origin based on uid and context. */
    public OfflinePageOrigin(Context context, int uid) {
        if (uid == Process.myUid()) {
            mAppName = "";
            mSignatures = null;
            return;
        }
        PackageManager pm = context.getPackageManager();
        String[] packages = pm.getPackagesForUid(uid);
        if (packages.length != 1) {
            mAppName = "";
            mSignatures = null;
        } else {
            mAppName = packages[0];
            mSignatures = getAppSignaturesFor(context, mAppName);
        }
    }

    /** Creates a Chrome origin. */
    public OfflinePageOrigin() {
        this("", null);
    }

    @VisibleForTesting
    OfflinePageOrigin(String appName, String[] signatures) {
        mAppName = appName;
        mSignatures = signatures;
    }

    /**
     * Encode the origin information into a JSON string of
     * [appName, [SHA-256 encoded signature, SHA-256 encoded signature...]]
     *
     * @return The JSON encoded origin information or empty string if there is
     *         no app information (ie assuming chrome).
     */
    public String encodeAsJsonString() {
        // We default to "", implying chrome-only if inputs invalid.
        if (isChrome()) return "";
        // JSONArray(Object[]) requires API 19
        JSONArray signatureArray = new JSONArray();
        for (String s : mSignatures) signatureArray.put(s);
        return new JSONArray().put(mAppName).put(signatureArray).toString();
    }

    /**
     * Returns whether the signature recorded in this origin matches the signature
     * in the context.
     *
     * Returns true if this origin is Chrome.
     */
    public boolean doesSignatureMatch(Context context) {
        String[] currentSignatures = getAppSignaturesFor(context, mAppName);
        return Arrays.equals(mSignatures, currentSignatures);
    }

    /** Returns whether this origin is chrome. */
    public boolean isChrome() {
        return TextUtils.isEmpty(mAppName) || mSignatures == null;
    }

    /** Returns the application package name of this origin. */
    public String getAppName() {
        return mAppName;
    }

    @Override
    public String toString() {
        return encodeAsJsonString();
    }

    @Override
    public boolean equals(Object other) {
        if (other != null && other instanceof OfflinePageOrigin) {
            OfflinePageOrigin o = (OfflinePageOrigin) other;
            return mAppName.equals(o.mAppName) && Arrays.equals(mSignatures, o.mSignatures);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Arrays.deepHashCode(new Object[] {mAppName, mSignatures});
    }

    /**
     * @param context The context to look up signatures.
     * @param appName The name of the application to look up.
     * @return a sorted list of strings representing the signatures of an app.
     *          Null if the app name is invalid or cannot be found.
     */
    @SuppressLint("PackageManagerGetSignatures")
    // https://stackoverflow.com/questions/39192844/android-studio-warning-when-using-packagemanager-get-signatures
    private static String[] getAppSignaturesFor(Context context, String appName) {
        if (TextUtils.isEmpty(appName)) return null;
        try {
            PackageManager packageManager = context.getPackageManager();
            Signature[] signatureList =
                    packageManager.getPackageInfo(appName, PackageManager.GET_SIGNATURES)
                            .signatures;
            MessageDigest messageDigest = MessageDigest.getInstance("SHA-256");
            String[] sigStrings = new String[signatureList.length];
            for (int i = 0; i < sigStrings.length; i++) {
                messageDigest.update(signatureList[i].toByteArray());

                // The digest is reset after completing the hash computation.
                sigStrings[i] = byteArrayToString(messageDigest.digest());
            }
            Arrays.sort(sigStrings);
            return sigStrings;
        } catch (NameNotFoundException e) {
            return null; // Cannot find the app anymore. No signatures.
        } catch (NoSuchAlgorithmException e) {
            return null; // Cannot find the SHA-256 encryption algorithm. Shouldn't happen.
        }
    }

    /**
     * Formats bytes into a string for easier comparison.
     *
     * @param input Input bytes.
     * @return A string representation of the input bytes, e.g., "0123456789abcdefg"
     */
    private static String byteArrayToString(byte[] input) {
        if (input == null) return null;

        return Base64.encodeToString(input, Base64.DEFAULT);
    }
}
