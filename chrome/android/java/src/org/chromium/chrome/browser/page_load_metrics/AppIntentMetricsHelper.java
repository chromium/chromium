// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_load_metrics;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** Helper to classify external application intent sources for UKM PageLoad metrics on Android. */
@JNINamespace("page_load_metrics")
@NullMarked
public class AppIntentMetricsHelper {
    // Matches ChromeTabCreator.UNKNOWN_APP_ID used when an external intent omits
    // Browser.EXTRA_APPLICATION_ID.
    private static final String UNKNOWN_APP_ID = "com.google.android.apps.chrome.unknown_app";

    // LINT.IfChange(AndroidExternalAppIntentCategory)
    @IntDef({
        AndroidExternalAppIntentCategory.NONE,
        AndroidExternalAppIntentCategory.CAMERA,
        AndroidExternalAppIntentCategory.EMAIL,
        AndroidExternalAppIntentCategory.NAVIGATION,
        AndroidExternalAppIntentCategory.GALLERY,
        AndroidExternalAppIntentCategory.MESSAGING,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AndroidExternalAppIntentCategory {
        int NONE = 0;
        int CAMERA = 1;
        int EMAIL = 2;
        int NAVIGATION = 3;
        int GALLERY = 4;
        int MESSAGING = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/enums.xml:AndroidExternalAppIntentCategory)

    // Top global apps whose package names do not end in a standard category leaf token.
    private static final Map<String, Integer> KNOWN_APP_CATEGORIES =
            Map.ofEntries(
                    // Email
                    Map.entry("ch.protonmail.android", AndroidExternalAppIntentCategory.EMAIL),
                    Map.entry("com.google.android.gm", AndroidExternalAppIntentCategory.EMAIL),
                    Map.entry(
                            "com.microsoft.office.outlook", AndroidExternalAppIntentCategory.EMAIL),
                    Map.entry(
                            "com.samsung.android.email.provider",
                            AndroidExternalAppIntentCategory.EMAIL),
                    // Navigation
                    Map.entry("com.nhn.android.nmap", AndroidExternalAppIntentCategory.NAVIGATION),
                    Map.entry("com.sygic.aura", AndroidExternalAppIntentCategory.NAVIGATION),
                    Map.entry("com.waze", AndroidExternalAppIntentCategory.NAVIGATION),
                    Map.entry("net.osmand", AndroidExternalAppIntentCategory.NAVIGATION),
                    Map.entry("net.osmand.plus", AndroidExternalAppIntentCategory.NAVIGATION),
                    // Messaging
                    Map.entry("com.discord", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.facebook.orca", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry(
                            "com.google.android.talk", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.instagram.android", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.kakao.talk", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.microsoft.teams", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.slack", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.snapchat.android", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.tencent.mm", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.tencent.mobileqq", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.viber.voip", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.whatsapp", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("com.whatsapp.w4b", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry("jp.naver.line.android", AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry(
                            "org.thoughtcrime.securesms",
                            AndroidExternalAppIntentCategory.MESSAGING),
                    Map.entry(
                            "org.thunderdog.challegram",
                            AndroidExternalAppIntentCategory.MESSAGING));

    private static final Set<String> EMAIL_LEAF_TOKENS = Set.of("email", "mail");
    private static final Set<String> NAVIGATION_LEAF_TOKENS =
            Set.of("map", "maps", "mapslite", "navi", "navigation", "navigator");
    private static final Set<String> GALLERY_LEAF_TOKENS =
            Set.of("gallery", "gallery3d", "photos", "photosgo");
    private static final Set<String> CAMERA_LEAF_TOKENS =
            Set.of(
                    "camera",
                    "camera2",
                    "camera3",
                    "camera5",
                    "cameralite",
                    "googlecamera",
                    "opencamera");
    private static final Set<String> MESSAGING_LEAF_TOKENS =
            Set.of("messaging", "messenger", "mms");

    private AppIntentMetricsHelper() {}

    /**
     * Inspects the given {@link WindowAndroid} and {@link Tab} to determine if the committed page
     * load was initiated by an external application intent, returning its category.
     */
    @CalledByNative
    static @AndroidExternalAppIntentCategory int getAppIntentCategory(
            @Nullable WindowAndroid windowAndroid, @Nullable Tab tab, boolean isFirstNavigation) {
        if (tab == null || tab.isDestroyed()) {
            return AndroidExternalAppIntentCategory.NONE;
        }

        Activity activity = windowAndroid != null ? windowAndroid.getActivity().get() : null;
        boolean isCustomTab = activity instanceof CustomTabActivity;
        boolean isExternalTab = tab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;
        if (!isCustomTab && (!isExternalTab || !isFirstNavigation)) {
            return AndroidExternalAppIntentCategory.NONE;
        }

        String currentPackage =
                ContextUtils.getApplicationContext() != null
                        ? ContextUtils.getApplicationContext().getPackageName()
                        : null;

        if (activity instanceof CustomTabActivity customTabActivity) {
            BrowserServicesIntentDataProvider dataProvider =
                    customTabActivity.getIntentDataProvider();
            if (dataProvider != null
                    && (dataProvider.isTrustedWebActivity()
                            || dataProvider.isWebappOrWebApkActivity()
                            || dataProvider.isAuthTab())) {
                return AndroidExternalAppIntentCategory.NONE;
            }
            String clientPkg = dataProvider != null ? dataProvider.getClientPackageName() : null;
            @AndroidExternalAppIntentCategory
            int clientCategory = classifyIfExternal(clientPkg, currentPackage);
            if (clientCategory != AndroidExternalAppIntentCategory.NONE) {
                return clientCategory;
            }
            Intent intent = customTabActivity.getIntent();
            if (intent == null || IntentHandler.wasIntentSenderChrome(intent)) {
                return AndroidExternalAppIntentCategory.NONE;
            }
            return classifyIntent(intent, customTabActivity, currentPackage);
        }

        // In ChromeTabbedActivity, rely strictly on per-tab attribution to avoid multi-tab
        // crosstalk from the singleton Activity intent.
        String tabAppId = tab.getUserDataHost() != null ? TabAssociatedApp.getAppId(tab) : null;
        @AndroidExternalAppIntentCategory
        int tabAppCategory = classifyIfExternal(tabAppId, currentPackage);
        if (tabAppCategory != AndroidExternalAppIntentCategory.NONE) {
            return tabAppCategory;
        }

        RedirectHandler redirectHandler =
                tab.getUserDataHost() != null ? RedirectHandlerTabHelper.getHandlerFor(tab) : null;
        Intent tabIntent = redirectHandler != null ? redirectHandler.getInitialIntent() : null;
        if (tabIntent == null || IntentHandler.wasIntentSenderChrome(tabIntent)) {
            return AndroidExternalAppIntentCategory.NONE;
        }
        return classifyIntent(tabIntent, /* activity= */ null, currentPackage);
    }

    /**
     * Checks each source-app signal on the intent in order and returns the first category found. A
     * signal naming an app outside the five categories does not stop the search.
     */
    private static @AndroidExternalAppIntentCategory int classifyIntent(
            Intent intent, @Nullable Activity activity, @Nullable String currentPackage) {
        for (String extraKey :
                new String[] {
                    Browser.EXTRA_APPLICATION_ID,
                    IntentHandler.EXTRA_LAUNCHED_FROM_PACKAGE,
                    IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE
                }) {
            @AndroidExternalAppIntentCategory
            int category =
                    classifyIfExternal(
                            IntentUtils.safeGetStringExtra(intent, extraKey), currentPackage);
            if (category != AndroidExternalAppIntentCategory.NONE) {
                return category;
            }
        }

        String activityReferrer =
                activity != null
                        ? IntentHandler.getActivityReferrer(intent, activity)
                        : IntentUtils.safeGetStringExtra(
                                intent, IntentHandler.EXTRA_ACTIVITY_REFERRER);
        @AndroidExternalAppIntentCategory
        int activityReferrerCategory =
                classifyIfExternal(
                        extractAndroidAppPackageFromReferrer(activityReferrer), currentPackage);
        if (activityReferrerCategory != AndroidExternalAppIntentCategory.NONE) {
            return activityReferrerCategory;
        }

        return classifyIfExternal(
                extractAndroidAppPackageFromReferrer(
                        IntentHandler.getReferrerUrlIncludingExtraHeaders(intent)),
                currentPackage);
    }

    private static @AndroidExternalAppIntentCategory int classifyIfExternal(
            @Nullable String candidatePackage, @Nullable String currentPackage) {
        return isExternalPackageName(candidatePackage, currentPackage)
                ? classifyPackage(candidatePackage)
                : AndroidExternalAppIntentCategory.NONE;
    }

    @Contract("null, _ -> false")
    private static boolean isExternalPackageName(
            @Nullable String candidatePackage, @Nullable String currentPackage) {
        return candidatePackage != null
                && !candidatePackage.isEmpty()
                && !candidatePackage.equals(currentPackage)
                && !candidatePackage.equals(UNKNOWN_APP_ID);
    }

    /** Extracts the host package name only from {@code android-app://<package>} referrer URIs. */
    private static @Nullable String extractAndroidAppPackageFromReferrer(
            @Nullable String referrer) {
        if (TextUtils.isEmpty(referrer)) {
            return null;
        }
        Uri uri = Uri.parse(referrer.trim());
        String scheme = uri.getScheme();
        if (scheme == null
                || !IntentUtils.ANDROID_APP_REFERRER_SCHEME.equals(scheme.toLowerCase(Locale.US))) {
            return null;
        }
        String host = uri.getHost();
        return TextUtils.isEmpty(host) ? null : host;
    }

    private static @AndroidExternalAppIntentCategory int classifyPackage(String packageName) {
        String normalized = packageName.toLowerCase(Locale.US);
        Integer known = KNOWN_APP_CATEGORIES.get(normalized);
        if (known != null) {
            return known;
        }

        // Match exact dot-delimited leaf token at segment index >= 2 so publisher domains
        // (e.g., jp.naver.*, ru.mail.*, com.camerasideas.*, com.dailymail.*) never false-positive.
        String[] segments = normalized.split("\\.");
        if (segments.length < 3) {
            return AndroidExternalAppIntentCategory.NONE;
        }
        String leaf = segments[segments.length - 1];
        if (EMAIL_LEAF_TOKENS.contains(leaf)) {
            return AndroidExternalAppIntentCategory.EMAIL;
        }
        if (NAVIGATION_LEAF_TOKENS.contains(leaf)) {
            return AndroidExternalAppIntentCategory.NAVIGATION;
        }
        if (GALLERY_LEAF_TOKENS.contains(leaf)) {
            return AndroidExternalAppIntentCategory.GALLERY;
        }
        if (CAMERA_LEAF_TOKENS.contains(leaf)) {
            return AndroidExternalAppIntentCategory.CAMERA;
        }
        if (MESSAGING_LEAF_TOKENS.contains(leaf)) {
            return AndroidExternalAppIntentCategory.MESSAGING;
        }
        return AndroidExternalAppIntentCategory.NONE;
    }
}
