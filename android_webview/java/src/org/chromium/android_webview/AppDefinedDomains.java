// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.verify.domain.DomainVerificationManager;
import android.content.pm.verify.domain.DomainVerificationUserState;
import android.content.res.Resources;
import android.content.res.XmlResourceParser;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Utility class for fetching different kinds of lists containing domains embedding apps have some
 * of relationship with (e.g. digital asset links, or web links).
 */
@JNINamespace("android_webview")
public class AppDefinedDomains {

    private static final String ASSET_STATEMENTS_IDENTIFIER = "asset_statements";
    private static final String TARGET_IDENTIFIER = "target";
    private static final String DOMAIN_IDENTIFIER = "site";
    private static final String INCLUDE_IDENTIFIER = "include";

    private static final String APP_MANIFEST = "AndroidManifest.xml";
    private static final String INTENT_ATTRIBUTE_ACTION = "action";
    private static final String INTENT_ATTRIBUTE_CATEGORY = "category";
    private static final String INTENT_ATTRIBUTE_DATA = "data";
    private static final String INTENT_ATTRIBUTE_SCHEME = "scheme";
    private static final String INTENT_ATTRIBUTE_HOST = "host";
    private static final String INTENT_ATTRIBUTE_NAME = "name";

    private static final String ACTION_VIEW = "android.intent.action.VIEW";
    private static final String CATEGORY_DEFAULT = "android.intent.category.DEFAULT";
    private static final String CATEGORY_BROWSABLE = "android.intent.category.BROWSABLE";
    private static final String SCHEME_HTTP = "http";
    private static final String SCHEME_HTTPS = "https";
    private static final String TAG_INTENT_FILTER = "intent-filter";
    private static final String TAG_MANIFEST = "manifest";
    private static final String ATTRIBUTE_PACKAGE = "package";
    private static final int MAX_COOKIE_VALUE = 20;

    @CalledByNative
    private static String[] getDomainsFromAssetStatements() {
        List<String> domains = fetchAssetStatementDomainsForApp();
        return domains.toArray(new String[0]);
    }

    @CalledByNative
    private static String[] getVerifiedDomainsFromAppLinks() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            // Cannot provide Android App Links information for API < 31
            return new String[0];
        }
        List<String> domains =
                fetchDomainsFromDomainVerificationManager(/* needsVerification= */ true);
        return domains.toArray(new String[0]);
    }

    @CalledByNative
    private static String[] getDomainsFromWebLinks() {
        List<String> domains;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            domains = fetchDomainsFromParsedManifest();
        } else {
            domains = fetchDomainsFromDomainVerificationManager(/* needsVerification= */ false);
        }
        return domains.toArray(new String[0]);
    }

    @CalledByNative
    private static String[] getDomainsFromAssetStatementsAndWebLinks() {
        List<String> domains = fetchAssetStatementDomainsForApp();
        // Includes both Web Links and Android App Links
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            domains.addAll(fetchDomainsFromParsedManifest());
        } else {
            domains.addAll(
                    fetchDomainsFromDomainVerificationManager(/* needsVerification= */ false));
        }
        return domains.toArray(new String[0]);
    }

    @CalledByNative
    private static String[] getIncludeLinksFromAssetStatements() {
        JSONArray statements = fetchAssetStatementsFromManifest();
        List<String> domains = new ArrayList<>();

        for (int i = 0; i < statements.length(); i++) {
            try {
                JSONObject statement = statements.getJSONObject(i);
                if (statement.has(INCLUDE_IDENTIFIER)) {
                    domains.add(statement.getString(INCLUDE_IDENTIFIER));
                }
            } catch (JSONException ignored) {
                // If an element is not an object, just ignore it.
            }
        }
        RecordHistogram.recordCount100Histogram(
                "Android.WebView.DigitalAssetLinks.AssetIncludes.LinksInManifest", domains.size());
        // Include statements should be limit to a maximum of 10 as per
        // https://developers.google.com/digital-asset-links/v1/statements#scaling-to-dozens-of-statements-or-more
        return domains.subList(0, Math.min(domains.size(), 10)).toArray(new String[0]);
    }

    private static Set<String> getHostsFromIntentFilter(XmlResourceParser parser)
            throws XmlPullParserException, IOException {
        Set<String> actions = new HashSet<>();
        Set<String> categories = new HashSet<>();
        Set<String> schemes = new HashSet<>();
        Set<String> hosts = new HashSet<>();
        int currDepth = parser.getDepth();

        while (parser.next() != XmlPullParser.END_DOCUMENT && parser.getDepth() > currDepth) {
            if (parser.getName() != null && parser.getName().equals(INTENT_ATTRIBUTE_ACTION)) {
                for (int i = 0; i < parser.getAttributeCount(); i++) {
                    if (parser.getAttributeName(i).equals(INTENT_ATTRIBUTE_NAME)) {
                        actions.add(parser.getAttributeValue(i));
                    }
                }
            }
            if (parser.getName() != null && parser.getName().equals(INTENT_ATTRIBUTE_CATEGORY)) {
                for (int i = 0; i < parser.getAttributeCount(); i++) {
                    if (parser.getAttributeName(i).equals(INTENT_ATTRIBUTE_NAME)) {
                        categories.add(parser.getAttributeValue(i));
                    }
                }
            }
            if (parser.getName() != null && parser.getName().equals(INTENT_ATTRIBUTE_DATA)) {
                for (int i = 0; i < parser.getAttributeCount(); i++) {
                    if (parser.getAttributeName(i).equals(INTENT_ATTRIBUTE_SCHEME)) {
                        schemes.add(parser.getAttributeValue(i));
                    }
                    if (parser.getAttributeName(i).equals(INTENT_ATTRIBUTE_HOST)) {
                        hosts.add(parser.getAttributeValue(i));
                    }
                }
            }
        }
        // Check if intent-filter satisfies the conditions for being a Web Link
        if (actions.contains(ACTION_VIEW)
                && categories.contains(CATEGORY_DEFAULT)
                && categories.contains(CATEGORY_BROWSABLE)
                && (schemes.contains(SCHEME_HTTP) || schemes.contains(SCHEME_HTTPS))) {
            return hosts;
        }
        return new HashSet<>();
    }

    private static boolean matchesPackageName(XmlResourceParser parserAtManifestTag) {
        for (int i = 0; i < parserAtManifestTag.getAttributeCount(); i++) {
            if (parserAtManifestTag.getAttributeName(i).equals(ATTRIBUTE_PACKAGE)) {
                // <manifest> tag is expected to have only one "package" attribute
                return parserAtManifestTag.getAttributeValue(i).equals(getPackageName());
            }
        }
        return false;
    }

    // Finds and returns the parser for the embedding app's AndroidManifest.
    // Iterates over a finite range of cookies to find the correct manifest. Returns null if
    // no manifest matches the package name of the embedding app.
    private static XmlResourceParser getManifestParser() {
        for (int assetCookie = 1; assetCookie < MAX_COOKIE_VALUE; assetCookie++) {
            XmlResourceParser parser;
            try {
                parser =
                        ContextUtils.getApplicationContext()
                                .getAssets()
                                .openXmlResourceParser(assetCookie, APP_MANIFEST);
            } catch (IOException e) {
                // try next assetCookie
                continue;
            }
            try {
                int type;
                while ((type = parser.next()) != XmlPullParser.END_DOCUMENT) {
                    if (parser.getDepth() == 1
                            && type == XmlPullParser.START_TAG
                            && parser.getName() != null
                            && parser.getName().equals(TAG_MANIFEST)) {
                        if (matchesPackageName(parser)) {
                            // return the open parser at the <manifest> tag
                            return parser;
                        } else {
                            // manifest can have only one <manifest> tag
                            break;
                        }
                    }
                }
            } catch (XmlPullParserException | IOException e) {
                // try next assetCookie
            }
            parser.close();
        }
        // could not find manifest with correct package name
        return null;
    }

    // Parse raw xml manifest and extract the web links from intent-filters. This is a
    // best effort solution for extracting web links for API < 31.
    // Returned list may not contain all Web Links if XML parsing fails midway.
    private static List<String> fetchDomainsFromParsedManifest() {
        XmlResourceParser parser = getManifestParser();
        if (parser == null) {
            return new ArrayList<>();
        }
        Set<String> hostDomains = new HashSet<>();
        int type;
        try {
            while ((type = parser.next()) != XmlPullParser.END_DOCUMENT) {
                if (type == XmlPullParser.START_TAG
                        && parser.getName() != null
                        && parser.getName().equals(TAG_INTENT_FILTER)) {
                    hostDomains.addAll(getHostsFromIntentFilter(parser));
                }
            }
        } catch (XmlPullParserException | IOException e) {
            // Return partially populated list
        }
        parser.close();
        return new ArrayList<>(hostDomains);
    }

    private static List<String> fetchAssetStatementDomainsForApp() {
        JSONArray statements;
        try {
            statements = fetchAssetStatementsFromManifest();
        } catch (Resources.NotFoundException e) {
            return new ArrayList<>();
        }
        List<String> domains = new ArrayList<>();
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

    private static JSONArray fetchAssetStatementsFromManifest() {
        ApplicationInfo appInfo;
        try {
            appInfo = getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            return new JSONArray();
        }
        if (appInfo == null || appInfo.metaData == null) {
            return new JSONArray();
        }

        int resourceIdentifier = appInfo.metaData.getInt(ASSET_STATEMENTS_IDENTIFIER);
        if (resourceIdentifier == 0) {
            return new JSONArray();
        }
        Resources resources;
        try {
            resources = getResourcesForApplication(appInfo);
        } catch (NameNotFoundException e) {
            return new JSONArray();
        }
        JSONArray statements;
        try {
            statements = new JSONArray(resources.getString(resourceIdentifier));
        } catch (Resources.NotFoundException | JSONException e) {
            return new JSONArray();
        }
        return statements;
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private static List<String> fetchDomainsFromDomainVerificationManager(
            boolean needsVerification) {
        DomainVerificationManager manager =
                ContextUtils.getApplicationContext()
                        .getSystemService(DomainVerificationManager.class);
        if (manager == null) {
            return new ArrayList<>();
        }
        DomainVerificationUserState userState;
        try {
            userState = manager.getDomainVerificationUserState(getPackageName());
        } catch (NameNotFoundException e) {
            return new ArrayList<>();
        }
        if (userState == null) {
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
