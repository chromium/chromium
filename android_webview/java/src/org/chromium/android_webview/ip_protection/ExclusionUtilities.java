// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.ip_protection;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.verify.domain.DomainVerificationManager;
import android.content.pm.verify.domain.DomainVerificationUserState;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.ContextUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Utility class for fetching different kinds of lists containing domains which have some form of
 * first-party relationship (e.g. digital asset links) with the embedding app.
 */
@JNINamespace("exclusion_utilities")
public class ExclusionUtilities {

    private static final String ASSET_STATEMENTS_IDENTIFIER = "asset_statements";
    private static final String TARGET_IDENTIFIER = "target";
    private static final String DOMAIN_IDENTIFIER = "site";

    @CalledByNative
    private static String[] getDomainsFromAssetStatements() {
        List<String> domains = fetchAssetStatementDomainsForApp();
        return domains.toArray(new String[domains.size()]);
    }

    @CalledByNative
    private static String[] getVerifiedDomainsFromAppLinks() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            // Cannot provide Android App Links information for API < 31
            return new String[0];
        }
        List<String> domains =
                fetchDomainsFromDomainVerificationManager(/* needsVerification= */ true);
        return domains.toArray(new String[domains.size()]);
    }

    @CalledByNative
    private static String[] getDomainsFromWebLinks() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            // Cannot fetch Web Links information for API < 31
            return new String[0];
        }
        List<String> domains =
                fetchDomainsFromDomainVerificationManager(/* needsVerification= */ false);
        return domains.toArray(new String[domains.size()]);
    }

    @CalledByNative
    private static String[] getDomainsFromAssetStatementsAndWebLinks() {
        List<String> domains = fetchAssetStatementDomainsForApp();
        // Includes both Web Links and Android App Links
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            domains.addAll(
                    fetchDomainsFromDomainVerificationManager(/* needsVerification= */ false));
        }
        return domains.toArray(new String[domains.size()]);
    }

    private static List<String> fetchAssetStatementDomainsForApp() {
        ApplicationInfo appInfo;
        Resources resources;
        JSONArray statements;
        List<String> domains = new ArrayList<String>();
        try {
            appInfo = getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            return new ArrayList<>();
        }
        if (appInfo == null || appInfo.metaData == null) {
            return new ArrayList<>();
        }

        int resourceIdentifier = appInfo.metaData.getInt(ASSET_STATEMENTS_IDENTIFIER);
        if (resourceIdentifier == 0) {
            return new ArrayList<>();
        }
        try {
            resources = getResourcesForApplication(appInfo);
        } catch (NameNotFoundException e) {
            return new ArrayList<>();
        }
        try {
            statements = new JSONArray(resources.getString(resourceIdentifier));
        } catch (Resources.NotFoundException | JSONException e) {
            return new ArrayList<>();
        }
        for (int i = 0; i < statements.length(); i++) {
            String site;
            try {
                site =
                        statements
                                .getJSONObject(i)
                                .getJSONObject(TARGET_IDENTIFIER)
                                .getString(DOMAIN_IDENTIFIER);
            } catch (JSONException e) {
                // If an element is not an object, just ignore it.
                continue;
            }
            if (site != null) {
                String host = Uri.parse(site).getHost();
                if (host != null) {
                    domains.add(host);
                }
            }
        }
        return domains;
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private static List<String> fetchDomainsFromDomainVerificationManager(
            boolean needsVerification) {
        DomainVerificationManager manager =
                ContextUtils.getApplicationContext()
                        .getSystemService(DomainVerificationManager.class);
        DomainVerificationUserState userState;
        try {
            userState = manager.getDomainVerificationUserState(getPackageName());
        } catch (NameNotFoundException e) {
            return new ArrayList<>();
        }
        Map<String, Integer> hostToStateMap = userState.getHostToStateMap();
        if (needsVerification) {
            hostToStateMap
                    .values()
                    .removeIf(value -> value != DomainVerificationUserState.DOMAIN_STATE_VERIFIED);
        }
        return new ArrayList<>(hostToStateMap.keySet());
    }

    private static String getPackageName() {
        return ContextUtils.getApplicationContext().getPackageName();
    }

    private static ApplicationInfo getApplicationInfo(String packageName, int flags)
            throws NameNotFoundException {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .getApplicationInfo(packageName, flags);
    }

    private static Resources getResourcesForApplication(ApplicationInfo appInfo)
            throws NameNotFoundException {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .getResourcesForApplication(appInfo);
    }
}
