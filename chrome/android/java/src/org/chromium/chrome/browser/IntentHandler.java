// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;

import android.app.Activity;
import android.app.KeyguardManager;
import android.app.PendingIntent;
import android.app.SearchManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;
import android.provider.MediaStore;
import android.speech.RecognizerResultsIntent;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.browserservices.BrowserSessionContentUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.externalnav.IntentWithGesturesHandler;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.rappor.RapporServiceBridge;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.content_public.common.Referrer;
import org.chromium.net.HttpUtil;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Handles all browser-related Intents.
 */
public class IntentHandler {
    private static final String TAG = "IntentHandler";

    /**
     * Document mode: If true, Chrome is launched into the same Task.
     * Note: used by first-party applications, do not rename.
     */
    public static final String EXTRA_APPEND_TASK = "com.android.chrome.append_task";

    /**
     * Document mode: If true, keep tasks in Recents when a user hits back at the root URL.
     * Note: used by first-party applications, do not rename.
     */
    public static final String EXTRA_PRESERVE_TASK = "com.android.chrome.preserve_task";

    /**
     * Document mode: If true, opens the document in background.
     * Note: used by first-party applications, do not rename.
     */
    public static final String EXTRA_OPEN_IN_BG = "com.android.chrome.open_with_affiliation";

    /**
     * Document mode: Records what caused a document to be created.
     */
    public static final String EXTRA_STARTED_BY = "com.android.chrome.started_by";

    /**
     * Tab ID to use when creating a new Tab.
     */
    public static final String EXTRA_TAB_ID = "com.android.chrome.tab_id";

    /**
     * Parcelable FullscreenOptions to use when creating a new Tab.
     */
    public static final String EXTRA_FULLSCREEN_OPTIONS = "com.android.chrome.fullscreen_options";

    /**
     * The tab id of the parent tab, if any.
     */
    public static final String EXTRA_PARENT_TAB_ID = "com.android.chrome.parent_tab_id";

    /**
     * Intent to bring the parent Activity back, if the parent Tab lives in a different Activity.
     */
    public static final String EXTRA_PARENT_INTENT = "com.android.chrome.parent_intent";

    /**
     * ComponentName of the parent Activity. Can be used by an Activity launched on top of another
     * Activity (e.g. BookmarkActivity) to intent back into the Activity it sits on top of.
     */
    public static final String EXTRA_PARENT_COMPONENT =
            "org.chromium.chrome.browser.parent_component";

    /**
     * Transition type is only set internally by a first-party app and has to be signed.
     */
    public static final String EXTRA_PAGE_TRANSITION_TYPE = "com.google.chrome.transition_type";

    /**
     * The original intent of the given intent before it was modified.
     */
    public static final String EXTRA_ORIGINAL_INTENT = "com.android.chrome.original_intent";

    /**
     * An extra to indicate that a particular intent was triggered from the first run experience
     * flow.
     */
    public static final String EXTRA_INVOKED_FROM_FRE = "com.android.chrome.invoked_from_fre";

    /**
     * An extra to indicate that the intent was triggered from a launcher shortcut.
     */
    public static final String EXTRA_INVOKED_FROM_SHORTCUT =
            "com.android.chrome.invoked_from_shortcut";

    /**
     * Intent extra used to identify the sending application.
     */
    private static final String TRUSTED_APPLICATION_CODE_EXTRA = "trusted_application_code_extra";

    /**
     * The scheme for referrer coming from an application.
     */
    public static final String ANDROID_APP_REFERRER_SCHEME = "android-app";

    /**
     * A referrer id used for Chrome to Chrome referrer passing.
     */
    public static final String EXTRA_REFERRER_ID = "org.chromium.chrome.browser.referrer_id";

    /**
     * An extra for identifying the referrer policy to be used.
     * TODO(yusufo): Move this to support library.
     */
    public static final String EXTRA_REFERRER_POLICY =
            "android.support.browser.extra.referrer_policy";

    /**
     * Key to associate a timestamp with an intent.
     */
    private static final String EXTRA_TIMESTAMP_MS = "org.chromium.chrome.browser.timestamp";

    /**
     * For multi-window, passes the id of the window.
     */
    public static final String EXTRA_WINDOW_ID = "org.chromium.chrome.browser.window_id";

    /**
     * Records package names of other applications in the system that could have handled
     * this intent.
     */
    public static final String EXTRA_EXTERNAL_NAV_PACKAGES = "org.chromium.chrome.browser.eenp";

    /**
     * Extra to indicate the launch type of the tab to be created.
     */
    private static final String EXTRA_TAB_LAUNCH_TYPE =
            "org.chromium.chrome.browser.tab_launch_type";

    /**
     * A hash code for the URL to verify intent data hasn't been modified.
     */
    public static final String EXTRA_DATA_HASH_CODE = "org.chromium.chrome.browser.data_hash";

    /**
     * A boolean to indicate whether incognito mode is currently selected.
     */
    public static final String EXTRA_INCOGNITO_MODE = "org.chromium.chrome.browser.incognito_mode";

    /**
     * Fake ComponentName used in constructing TRUSTED_APPLICATION_CODE_EXTRA.
     */
    private static ComponentName sFakeComponentName;

    private static final Object LOCK = new Object();

    private static Pair<Integer, String> sPendingReferrer;
    private static int sReferrerId;
    private static String sPendingIncognitoUrl;

    private static final String PACKAGE_GSA = "com.google.android.googlequicksearchbox";
    private static final String PACKAGE_GMAIL = "com.google.android.gm";
    private static final String PACKAGE_PLUS = "com.google.android.apps.plus";
    private static final String PACKAGE_HANGOUTS = "com.google.android.talk";
    private static final String PACKAGE_MESSENGER = "com.google.android.apps.messaging";
    private static final String PACKAGE_LINE = "jp.naver.line.android";
    private static final String PACKAGE_WHATSAPP = "com.whatsapp";
    private static final String PACKAGE_YAHOO_MAIL = "com.yahoo.mobile.client.android.mail";
    private static final String PACKAGE_VIBER = "com.viber.voip";
    private static final String FACEBOOK_REFERRER_URL = "android-app://m.facebook.com";
    private static final String FACEBOOK_INTERNAL_BROWSER_REFERRER = "http://m.facebook.com";
    private static final String TWITTER_LINK_PREFIX = "http://t.co/";
    private static final String NEWS_LINK_PREFIX = "http://news.google.com/news/url?";

    /**
     * Represents popular external applications that can load a page in Chrome via intent.
     * DO NOT reorder items in this interface, because it's mirrored to UMA (as ClientAppId).
     * Values should be enumerated from 0 and can't have gaps. When removing items,
     * comment them out and keep existing numeric values stable.
     */
    @IntDef({ExternalAppId.OTHER, ExternalAppId.GMAIL, ExternalAppId.FACEBOOK, ExternalAppId.PLUS,
            ExternalAppId.TWITTER, ExternalAppId.CHROME, ExternalAppId.HANGOUTS,
            ExternalAppId.MESSENGER, ExternalAppId.NEWS, ExternalAppId.LINE, ExternalAppId.WHATSAPP,
            ExternalAppId.GSA, ExternalAppId.WEBAPK, ExternalAppId.YAHOO_MAIL, ExternalAppId.VIBER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ExternalAppId {
        int OTHER = 0;
        int GMAIL = 1;
        int FACEBOOK = 2;
        int PLUS = 3;
        int TWITTER = 4;
        int CHROME = 5;
        int HANGOUTS = 6;
        int MESSENGER = 7;
        int NEWS = 8;
        int LINE = 9;
        int WHATSAPP = 10;
        int GSA = 11;
        int WEBAPK = 12;
        int YAHOO_MAIL = 13;
        int VIBER = 14;
        // Update ClientAppId in enums.xml when adding new items.
        int NUM_ENTRIES = 15;
    }

    private static ComponentName getFakeComponentName(String packageName) {
        synchronized (LOCK) {
            if (sFakeComponentName == null) {
                sFakeComponentName = new ComponentName(packageName, "FakeClass");
            }
        }

        return sFakeComponentName;
    }

    /** Intent extra to open an incognito tab. */
    public static final String EXTRA_OPEN_NEW_INCOGNITO_TAB =
            "com.google.android.apps.chrome.EXTRA_OPEN_NEW_INCOGNITO_TAB";

    /** Schemes used by web pages to start up Chrome without an explicit Intent. */
    public static final String GOOGLECHROME_SCHEME = "googlechrome";
    public static final String GOOGLECHROME_NAVIGATE_PREFIX =
            GOOGLECHROME_SCHEME + "://navigate?url=";

    private static boolean sTestIntentsEnabled;

    private final IntentHandlerDelegate mDelegate;
    private final String mPackageName;

    /**
     * Receiver for screen unlock broadcast.
     */
    private DelayedScreenLockIntentHandler mDelayedScreenIntentHandler;

    @IntDef({TabOpenType.OPEN_NEW_TAB, TabOpenType.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB,
            TabOpenType.REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB, TabOpenType.CLOBBER_CURRENT_TAB,
            TabOpenType.BRING_TAB_TO_FRONT, TabOpenType.OPEN_NEW_INCOGNITO_TAB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabOpenType {
        int OPEN_NEW_TAB = 0;
        // Tab is reused only if the URLs perfectly match.
        int REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB = 1;
        // Tab is reused only if there's an existing tab opened by the same app ID.
        int REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB = 2;
        int CLOBBER_CURRENT_TAB = 3;
        int BRING_TAB_TO_FRONT = 4;
        // Opens a new incognito tab.
        int OPEN_NEW_INCOGNITO_TAB = 5;

        String BRING_TAB_TO_FRONT_STRING = "BRING_TAB_TO_FRONT";
    }

    /**
     * A delegate interface for users of IntentHandler.
     */
    public static interface IntentHandlerDelegate {
        /**
         * Processes a URL VIEW Intent.
         */
        void processUrlViewIntent(String url, String referer, String headers,
                @TabOpenType int tabOpenType, String externalAppId, int tabIdToBringToFront,
                boolean hasUserGesture, Intent intent);

        void processWebSearchIntent(String query);
    }

    /** Sets whether or not test intents are enabled. */
    @VisibleForTesting
    public static void setTestIntentsEnabled(boolean enabled) {
        sTestIntentsEnabled = enabled;
    }

    public IntentHandler(IntentHandlerDelegate delegate, String packageName) {
        mDelegate = delegate;
        mPackageName = packageName;
    }

    /**
     * Determines what App was used to fire this Intent.
     * @param intent Intent that was used to launch Chrome.
     * @return ExternalAppId representing the app.
     */
    public static @ExternalAppId int determineExternalIntentSource(Intent intent) {
        String appId = IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID);
        @ExternalAppId
        int externalId = ExternalAppId.OTHER;
        if (appId == null) {
            String url = getUrlFromIntent(intent);
            String referrer = getReferrerUrl(intent);
            if (url != null && url.startsWith(TWITTER_LINK_PREFIX)) {
                externalId = ExternalAppId.TWITTER;
            } else if (FACEBOOK_REFERRER_URL.equals(referrer)) {
                // This happens when "Links Open Externally" is checked in the Facebook app.
                externalId = ExternalAppId.FACEBOOK;
            } else if (url != null && url.startsWith(NEWS_LINK_PREFIX)) {
                externalId = ExternalAppId.NEWS;
            } else {
                Bundle headers = IntentUtils.safeGetBundleExtra(intent, Browser.EXTRA_HEADERS);
                if (headers != null
                        && FACEBOOK_INTERNAL_BROWSER_REFERRER.equals(headers.get("Referer"))) {
                    // This happens when "Links Open Externally" is unchecked in the Facebook app,
                    // and we use "Open With..." from the internal browser.
                    externalId = ExternalAppId.FACEBOOK;
                }
            }
        } else {
            externalId = mapPackageToExternalAppId(appId);
        }
        return externalId;
    }

    /**
     * Returns the appropriate entry of the ExteranAppId enum based on the supplied package name.
     * @param packageName String The application package name to map.
     * @return ExternalAppId representing the app.
     */
    public static @ExternalAppId int mapPackageToExternalAppId(String packageName) {
        if (packageName.equals(PACKAGE_PLUS)) {
            return ExternalAppId.PLUS;
        } else if (packageName.equals(PACKAGE_GMAIL)) {
            return ExternalAppId.GMAIL;
        } else if (packageName.equals(PACKAGE_HANGOUTS)) {
            return ExternalAppId.HANGOUTS;
        } else if (packageName.equals(PACKAGE_MESSENGER)) {
            return ExternalAppId.MESSENGER;
        } else if (packageName.equals(PACKAGE_LINE)) {
            return ExternalAppId.LINE;
        } else if (packageName.equals(PACKAGE_WHATSAPP)) {
            return ExternalAppId.WHATSAPP;
        } else if (packageName.equals(PACKAGE_GSA)) {
            return ExternalAppId.GSA;
        } else if (packageName.equals(ContextUtils.getApplicationContext().getPackageName())) {
            return ExternalAppId.CHROME;
        } else if (packageName.startsWith(WEBAPK_PACKAGE_PREFIX)) {
            return ExternalAppId.WEBAPK;
        } else if (packageName.equals(PACKAGE_YAHOO_MAIL)) {
            return ExternalAppId.YAHOO_MAIL;
        } else if (packageName.equals(PACKAGE_VIBER)) {
            return ExternalAppId.VIBER;
        }
        return ExternalAppId.OTHER;
    }

    private void recordExternalIntentSourceUMA(Intent intent) {
        @ExternalAppId
        int externalId = determineExternalIntentSource(intent);
        RecordHistogram.recordEnumeratedHistogram(
                "MobileIntent.PageLoadDueToExternalApp", externalId, ExternalAppId.NUM_ENTRIES);
        if (externalId == ExternalAppId.OTHER) {
            String appId = IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID);
            if (!TextUtils.isEmpty(appId)) {
                RapporServiceBridge.sampleString("Android.PageLoadDueToExternalApp", appId);
            }
        }
    }

    /**
     * Records an action when a user chose to handle a URL in Chrome that could have been handled
     * by an application installed on the phone. Also records the name of that application.
     * This doesn't include generic URL handlers, such as browsers.
     */
    private void recordAppHandlersForIntent(Intent intent) {
        List<String> packages = IntentUtils.safeGetStringArrayListExtra(intent,
                IntentHandler.EXTRA_EXTERNAL_NAV_PACKAGES);
        if (packages != null && packages.size() > 0) {
            RecordUserAction.record("MobileExternalNavigationReceived");
            for (String name : packages) {
                RapporServiceBridge.sampleString("Android.ExternalNavigationNotChosen", name);
            }
        }
    }

    private void updateDeferredIntent(Intent intent) {
        if (mDelayedScreenIntentHandler == null && intent != null) {
            mDelayedScreenIntentHandler = new DelayedScreenLockIntentHandler();
        }

        if (mDelayedScreenIntentHandler != null) {
            mDelayedScreenIntentHandler.updateDeferredIntent(intent);
        }
    }

    /**
     * Handles an Intent after the ChromeTabbedActivity decides that it shouldn't ignore the
     * Intent.
     * @param intent Target intent.
     * @return Whether the Intent was successfully handled.
     */
    boolean onNewIntent(Intent intent) {
        updateDeferredIntent(null);

        assert intentHasValidUrl(intent);
        String url = getUrlFromIntent(intent);
        boolean hasUserGesture =
                IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent);
        @TabOpenType
        int tabOpenType = getTabOpenType(intent);
        int tabIdToBringToFront = IntentUtils.safeGetIntExtra(
                intent, TabOpenType.BRING_TAB_TO_FRONT_STRING, Tab.INVALID_TAB_ID);
        if (url == null && tabIdToBringToFront == Tab.INVALID_TAB_ID
                && tabOpenType != TabOpenType.OPEN_NEW_INCOGNITO_TAB) {
            return handleWebSearchIntent(intent);
        }

        String referrerUrl = getReferrerUrlIncludingExtraHeaders(intent);
        String extraHeaders = getExtraHeadersFromIntent(intent, true);

        if (isIntentForMhtmlFileOrContent(intent) && tabOpenType == TabOpenType.OPEN_NEW_TAB
                && referrerUrl == null && extraHeaders == null) {
            handleMhtmlFileOrContentIntent(url, intent);
            return true;
        }

        processUrlViewIntent(url, referrerUrl, extraHeaders, tabOpenType,
                IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID),
                tabIdToBringToFront, hasUserGesture, intent);
        return true;
    }

    private void processUrlViewIntent(String url, String referrerUrl, String extraHeaders,
            @TabOpenType int tabOpenType, String externalAppId, int tabIdToBringToFront,
            boolean hasUserGesture, Intent intent) {
        extraHeaders = maybeAddAdditionalExtraHeaders(intent, url, extraHeaders);

        // TODO(joth): Presumably this should check the action too.
        mDelegate.processUrlViewIntent(url, referrerUrl, extraHeaders, tabOpenType,
                IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID),
                tabIdToBringToFront, hasUserGesture, intent);
        recordExternalIntentSourceUMA(intent);
        recordAppHandlersForIntent(intent);
    }

    /**
     * Extracts referrer Uri from intent, if supplied.
     * @param intent The intent to use.
     * @return The referrer Uri.
     */
    private static Uri getReferrer(Intent intent) {
        Uri referrer = IntentUtils.safeGetParcelableExtra(intent, Intent.EXTRA_REFERRER);
        if (referrer != null) {
            return referrer;
        }
        String referrerName = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_REFERRER_NAME);
        if (referrerName != null) {
            return Uri.parse(referrerName);
        }
        return null;
    }

    /**
     * Extracts referrer URL string. The extra is used if we received it from a first party app or
     * if the referrer_extra is specified as android-app://package style URL.
     * @param intent The intent from which to extract the URL.
     * @return The URL string or null if none should be used.
     */
    private static String getReferrerUrl(Intent intent) {
        Uri referrerExtra = getReferrer(intent);
        if (referrerExtra == null) return null;
        String referrerUrl = IntentHandler.getPendingReferrerUrl(
                IntentUtils.safeGetIntExtra(intent, EXTRA_REFERRER_ID, 0));
        if (!TextUtils.isEmpty(referrerUrl)) {
            return referrerUrl;
        } else if (isValidReferrerHeader(referrerExtra)) {
            return referrerExtra.toString();
        } else if (IntentHandler.notSecureIsIntentChromeOrFirstParty(intent)
                || BrowserSessionContentUtils.canActiveContentHandlerUseReferrer(
                           intent, referrerExtra)) {
            return referrerExtra.toString();
        }
        return null;
    }

    /**
     * Gets the referrer, looking in the Intent extra and in the extra headers extra.
     *
     * The referrer extra takes priority over the "extra headers" one.
     *
     * @param intent The Intent containing the extras.
     * @return The referrer, or null.
     */
    public static String getReferrerUrlIncludingExtraHeaders(Intent intent) {
        String referrerUrl = getReferrerUrl(intent);
        if (referrerUrl != null) return referrerUrl;

        Bundle bundleExtraHeaders = IntentUtils.safeGetBundleExtra(intent, Browser.EXTRA_HEADERS);
        if (bundleExtraHeaders == null) return null;
        for (String key : bundleExtraHeaders.keySet()) {
            String value = bundleExtraHeaders.getString(key);
            if (value != null && "referer".equals(key.toLowerCase(Locale.US))) {
                Uri referrer = Uri.parse(value).normalizeScheme();
                if (isValidReferrerHeader(referrer)) return referrer.toString();
            }
        }
        return null;
    }

    /**
     * Add referrer and extra headers to a {@link LoadUrlParams}, if we managed to parse them from
     * the intent.
     * @param params The {@link LoadUrlParams} to add referrer and headers.
     * @param intent The intent we use to parse the extras.
     */
    public static void addReferrerAndHeaders(LoadUrlParams params, Intent intent) {
        String referrer = getReferrerUrlIncludingExtraHeaders(intent);
        if (referrer != null) {
            params.setReferrer(new Referrer(referrer, getReferrerPolicyFromIntent(intent)));
        }
        String headers = getExtraHeadersFromIntent(intent, true);
        if (headers != null) params.setVerbatimHeaders(headers);
    }

    public static int getReferrerPolicyFromIntent(Intent intent) {
        int policy =
                IntentUtils.safeGetIntExtra(intent, EXTRA_REFERRER_POLICY, ReferrerPolicy.DEFAULT);
        if (policy < 0 || policy >= ReferrerPolicy.LAST) {
            policy = ReferrerPolicy.DEFAULT;
        }
        return policy;
    }

    /**
     * @return Whether that the given referrer is of the format that Chrome allows external
     * apps to specify.
     */
    private static boolean isValidReferrerHeader(Uri referrer) {
        if (referrer == null) return false;
        Uri normalized = referrer.normalizeScheme();
        return TextUtils.equals(normalized.getScheme(), ANDROID_APP_REFERRER_SCHEME)
                && !TextUtils.isEmpty(normalized.getHost());
    }

    /**
     * Constructs a valid referrer using the given authority.
     * @param authority The authority to use.
     * @return Referrer with default policy that uses the valid android app scheme, or null.
     */
    public static Referrer constructValidReferrerForAuthority(String authority) {
        if (TextUtils.isEmpty(authority)) return null;
        return new Referrer(new Uri.Builder()
                                    .scheme(ANDROID_APP_REFERRER_SCHEME)
                                    .authority(authority)
                                    .build()
                                    .toString(),
                ReferrerPolicy.DEFAULT);
    }

    /**
     * Extracts the URL from voice search result intent.
     * @return URL if it was found, null otherwise.
     */
    static String getUrlFromVoiceSearchResult(Intent intent) {
        if (!RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS.equals(intent.getAction())) {
            return null;
        }
        ArrayList<String> results = IntentUtils.safeGetStringArrayListExtra(
                intent, RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS);

        // Allow specifying a single voice result via the command line during testing (as the
        // 'am' command does not allow specifying an array of strings).
        if (results == null && sTestIntentsEnabled) {
            String testResult = IntentUtils.safeGetStringExtra(
                    intent, RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS);
            if (testResult != null) {
                results = new ArrayList<String>();
                results.add(testResult);
            }
        }
        if (results == null || results.size() == 0
                || !BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isStartupSuccessfullyCompleted()) {
            return null;
        }
        String query = results.get(0);
        String url = AutocompleteController.nativeQualifyPartialURLQuery(query);
        if (url == null) {
            List<String> urls = IntentUtils.safeGetStringArrayListExtra(
                    intent, RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS);
            if (urls != null && urls.size() > 0) {
                url = urls.get(0);
            } else {
                url = TemplateUrlService.getInstance().getUrlForVoiceSearchQuery(query);
            }
        }
        return url;
    }

    public boolean handleWebSearchIntent(Intent intent) {
        if (intent == null) return false;

        String query = null;
        final String action = intent.getAction();
        if (Intent.ACTION_SEARCH.equals(action)
                || MediaStore.INTENT_ACTION_MEDIA_SEARCH.equals(action)) {
            query = IntentUtils.safeGetStringExtra(intent, SearchManager.QUERY);
        }

        if (query == null || TextUtils.isEmpty(query)) return false;

        mDelegate.processWebSearchIntent(query);
        return true;
    }

    private void handleMhtmlFileOrContentIntent(final String url, final Intent intent) {
        OfflinePageUtils.getLoadUrlParamsForOpeningMhtmlFileOrContent(url, (loadUrlParams) -> {
            processUrlViewIntent(loadUrlParams.getUrl(), null, loadUrlParams.getVerbatimHeaders(),
                    TabOpenType.OPEN_NEW_TAB, null, 0, false, intent);
        });
    }

    private static PendingIntent getAuthenticationToken() {
        Intent fakeIntent = new Intent();
        Context appContext = ContextUtils.getApplicationContext();
        fakeIntent.setComponent(getFakeComponentName(appContext.getPackageName()));
        return PendingIntent.getActivity(appContext, 0, fakeIntent, 0);
    }

    /**
     * Start activity for the given trusted Intent.
     *
     * To make sure the intent is not dropped by Chrome, we send along an authentication token to
     * identify ourselves as a trusted sender. The method {@link #shouldIgnoreIntent} validates the
     * token.
     */
    public static void startActivityForTrustedIntent(Intent intent) {
        startActivityForTrustedIntentInternal(intent, null);
    }

    /**
     * Start the activity that handles launching tabs in Chrome given the trusted intent.
     *
     * This allows specifying URLs that chrome:// handles internally, but does not expose in
     * intent-filters for global use.
     *
     * To make sure the intent is not dropped by Chrome, we send along an authentication token to
     * identify ourselves as a trusted sender. The method {@link #shouldIgnoreIntent} validates the
     * token.
     */
    public static void startChromeLauncherActivityForTrustedIntent(Intent intent) {
        // Specify the exact component that will handle creating a new tab.  This allows specifying
        // URLs that are not exposed in the intent filters (i.e. chrome://).
        startActivityForTrustedIntentInternal(intent, ChromeLauncherActivity.class.getName());
    }

    private static void startActivityForTrustedIntentInternal(
            Intent intent, String componentClassName) {
        Context appContext = ContextUtils.getApplicationContext();
        // The caller might want to re-use the Intent, so we'll use a copy.
        Intent copiedIntent = new Intent(intent);

        if (componentClassName != null) {
            assert copiedIntent.getComponent() == null;
            // Specify the exact component that will handle creating a new tab.  This allows
            // specifying URLs that are not exposed in the intent filters (i.e. chrome://).
            copiedIntent.setComponent(
                    new ComponentName(appContext.getPackageName(), componentClassName));
        }

        // Because we are starting this activity from the application context, we need
        // FLAG_ACTIVITY_NEW_TASK on pre-N versions of Android.  On N+ we can get away with
        // specifying a task ID or not specifying an options bundle.
        assert (copiedIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0;
        addTrustedIntentExtras(copiedIntent);
        appContext.startActivity(copiedIntent);
    }

    /**
     * Sets TRUSTED_APPLICATION_CODE_EXTRA on the provided intent to identify it as coming from
     * a trusted source.
     */
    public static void addTrustedIntentExtras(Intent intent) {
        if (ExternalNavigationDelegateImpl.willChromeHandleIntent(intent, true)) {
            // It is crucial that we never leak the authentication token to other packages, because
            // then the other package could be used to impersonate us/do things as us. Therefore,
            // scope the real Intent to our package.
            intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
            // The PendingIntent functions as an authentication token --- it could only have come
            // from us. Stash it in the real Intent as an extra. shouldIgnoreIntent will retrieve it
            // and check it with isIntentChromeInternal.
            intent.putExtra(TRUSTED_APPLICATION_CODE_EXTRA, getAuthenticationToken());
        }
    }

    /**
     * Calls {@link #getExtraHeadersFromIntent(Intent, boolean)} with shouldLogHeaders as false.
     */
    public static String getExtraHeadersFromIntent(Intent intent) {
        return getExtraHeadersFromIntent(intent, false);
    }

    /**
     * Returns a String (or null) containing the extra headers sent by the intent, if any.
     *
     * This methods skips the referrer header.
     *
     * @param intent The intent containing the bundle extra with the HTTP headers.
     * @param shouldLogHeaders Whether we should perform logging on the types of headers that the
     *                         Intent contains. This should only be done for Intents as they come
     *                         in to Chrome.
     */
    public static String getExtraHeadersFromIntent(Intent intent, boolean shouldLogHeaders) {
        Bundle bundleExtraHeaders = IntentUtils.safeGetBundleExtra(intent, Browser.EXTRA_HEADERS);
        if (bundleExtraHeaders == null) return null;
        StringBuilder extraHeaders = new StringBuilder();

        // We do some logging to determine what kinds of headers developers are inserting.
        IntentHeadersRecorder recorder = shouldLogHeaders ? new IntentHeadersRecorder() : null;

        for (String key : bundleExtraHeaders.keySet()) {
            String value = bundleExtraHeaders.getString(key);

            // Strip the custom header that can only be added by ourselves.
            if ("x-chrome-intent-type".equals(key.toLowerCase(Locale.US))) continue;

            if (shouldLogHeaders) recorder.recordHeader(key, value);

            if (!HttpUtil.isAllowedHeader(key, value)) continue;

            if (extraHeaders.length() != 0) extraHeaders.append("\n");
            extraHeaders.append(key);
            extraHeaders.append(": ");
            extraHeaders.append(value);
        }

        if (shouldLogHeaders) {
            recorder.report(IntentHandler.notSecureIsIntentChromeOrFirstParty(intent));
        }
        return extraHeaders.length() == 0 ? null : extraHeaders.toString();
    }

    /**
     * Adds a timestamp to an intent, as returned by {@link SystemClock#elapsedRealtime()}.
     *
     * To track page load time, this needs to be called as close as possible to
     * the entry point (in {@link Activity#onCreate()} for instance).
     */
    public static void addTimestampToIntent(Intent intent) {
        addTimestampToIntent(intent, SystemClock.elapsedRealtime());
    }

    /**
     * Adds provided timestamp to an intent.
     *
     * To track page load time, the value passed in should be as close as possible to
     * the entry point (in {@link Activity#onCreate()} for instance).
     */
    public static void addTimestampToIntent(Intent intent, long timeStamp) {
        intent.putExtra(EXTRA_TIMESTAMP_MS, timeStamp);
    }

    /**
     * @return the timestamp associated with an intent, or -1.
     */
    public static long getTimestampFromIntent(Intent intent) {
        return intent.getLongExtra(EXTRA_TIMESTAMP_MS, -1);
    }

    /**
     * Adds provided WebAPK's shell launch timestamp to an intent.
     */
    public static void addShellLaunchTimestampToIntent(Intent intent, long timestamp) {
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, timestamp);
    }

    /**
     * @return the WebAPK's shell launch timestamp associated with an intent, or -1.
     */
    public static long getWebApkShellLaunchTimestampFromIntent(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, -1);
    }

    /**
     * Returns true if the app should ignore a given intent.
     *
     * @param intent Intent to check.
     * @return true if the intent should be ignored.
     */
    public boolean shouldIgnoreIntent(Intent intent) {
        // Although not documented to, many/most methods that retrieve values from an Intent may
        // throw. Because we can't control what packages might send to us, we should catch any
        // Throwable and then fail closed (safe). This is ugly, but resolves top crashers in the
        // wild.
        try {
            // Ignore all invalid URLs, regardless of what the intent was.
            if (!intentHasValidUrl(intent)) {
                return true;
            }

            // Determine if this intent came from a trustworthy source (either Chrome or Google
            // first party applications).
            boolean isInternal = notSecureIsIntentChromeOrFirstParty(intent);
            boolean isFromChrome = wasIntentSenderChrome(intent);

            // "Open new incognito tab" is currently limited to Chrome.
            //
            // The pending incognito URL check is to handle the case where the user is shown an
            // Android intent picker while in incognito and they select the current Chrome instance
            // from the list.  In this case, we do not apply our Chrome token as the user has the
            // option to select apps outside of our control, so we rely on this in memory check
            // instead.
            if (!isFromChrome
                    && IntentUtils.safeGetBooleanExtra(
                            intent, EXTRA_OPEN_NEW_INCOGNITO_TAB, false)
                    && (getPendingIncognitoUrl() == null
                            || !getPendingIncognitoUrl().equals(intent.getDataString()))) {
                return true;
            }

            // Now if we have an empty URL and the intent was ACTION_MAIN,
            // we are pretty sure it is the launcher calling us to show up.
            // We can safely ignore the screen state.
            String url = getUrlFromIntent(intent);
            if (url == null && Intent.ACTION_MAIN.equals(intent.getAction())) {
                return false;
            }

            // Ignore all intents that specify a Chrome internal scheme if they did not come from
            // a trustworthy source.
            String scheme = getSanitizedUrlScheme(url);
            if (!isInternal && scheme != null
                    && (intent.hasCategory(Intent.CATEGORY_BROWSABLE)
                               || intent.hasCategory(Intent.CATEGORY_DEFAULT)
                               || intent.getCategories() == null)) {
                String lowerCaseScheme = scheme.toLowerCase(Locale.US);
                if (UrlConstants.CHROME_SCHEME.equals(lowerCaseScheme)
                        || UrlConstants.CHROME_NATIVE_SCHEME.equals(lowerCaseScheme)
                        || ContentUrlConstants.ABOUT_SCHEME.equals(lowerCaseScheme)) {
                    // Allow certain "safe" internal URLs to be launched by external
                    // applications.
                    String lowerCaseUrl = url.toLowerCase(Locale.US);
                    if (ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(lowerCaseUrl)
                            || ContentUrlConstants.ABOUT_BLANK_URL.equals(lowerCaseUrl)) {
                        return false;
                    }

                    Log.w(TAG, "Ignoring internal Chrome URL from untrustworthy source.");
                    return true;
                }
            }

            // We must check for screen state at this point.
            // These might be slow.
            boolean internalOrVisible = isInternal || isIntentUserVisible();
            if (!internalOrVisible) {
                updateDeferredIntent(intent);
                return true;
            }
            return false;
        } catch (Throwable t) {
            return true;
        }
    }

    @VisibleForTesting
    boolean intentHasValidUrl(Intent intent) {
        String url = extractUrlFromIntent(intent);

        // Check if this is a valid googlechrome:// URL.
        if (isGoogleChromeScheme(url)) {
            url = getUrlFromGoogleChromeSchemeUrl(url);
            if (url == null) return false;
        }

        // Always drop insecure urls.
        if (url != null && isJavascriptSchemeOrInvalidUrl(url)) {
            return false;
        }

        return true;
    }

    /**
     * Fetch the authentication token (a PendingIntent) created by startActivityForTrustedIntent,
     * if any. If anything goes wrong trying to retrieve the token (examples include
     * BadParcelableException or ClassNotFoundException), fail closed.
     */
    private static PendingIntent fetchAuthenticationTokenFromIntent(Intent intent) {
        return (PendingIntent) IntentUtils.safeGetParcelableExtra(
                intent, TRUSTED_APPLICATION_CODE_EXTRA);
    }

    private static boolean isChromeToken(PendingIntent token) {
        // Fetch what should be a matching token.
        PendingIntent pending = getAuthenticationToken();
        return pending.equals(token);
    }

    /**
     * @param intent An Intent to be checked.
     * @return Whether an intent originates from Chrome.
     */
    public static boolean wasIntentSenderChrome(Intent intent) {
        if (intent == null) return false;

        PendingIntent token = fetchAuthenticationTokenFromIntent(intent);
        if (token == null) return false;

        // Do not ignore a valid URL Intent if the sender is Chrome. (If the PendingIntents are
        // equal, we know that the sender was us.)
        return isChromeToken(token);
    }

    /**
     * Attempts to verify that an Intent was sent from either Chrome or a first-
     * party app by evaluating a PendingIntent token within the passed Intent.
     *
     * This method of verifying first-party apps is not secure, as it is not
     * possible to determine the sender of an Intent. This method only verifies
     * the creator of the PendingIntent token. But a malicious app may be able
     * to obtain a PendingIntent from another application and use it to
     * masquerade as it for the purposes of this check. Do not use this method.
     *
     * @param intent An Intent to be checked.
     * @return Whether an intent originates from Chrome or a first-party app.
     *
     * @deprecated This method is not reliable, see https://crbug.com/832124
     */
    @Deprecated
    public static boolean notSecureIsIntentChromeOrFirstParty(Intent intent) {
        if (intent == null) return false;

        PendingIntent token = fetchAuthenticationTokenFromIntent(intent);
        if (token == null) return false;

        // Do not ignore a valid URL Intent if the sender is Chrome. (If the PendingIntents are
        // equal, we know that the sender was us.)
        if (isChromeToken(token)) {
            return true;
        }
        if (ExternalAuthUtils.getInstance().isGoogleSigned(
                    ApiCompatibilityUtils.getCreatorPackage(token))) {
            return true;
        }
        return false;
    }

    @VisibleForTesting
    boolean isIntentUserVisible() {
        // Only process Intents if the screen is on and the device is unlocked;
        // i.e. the user will see what is going on.
        Context appContext = ContextUtils.getApplicationContext();
        if (!ApiCompatibilityUtils.isInteractive(appContext)) return false;
        if (!ApiCompatibilityUtils.isDeviceProvisioned(appContext)) return true;
        return !((KeyguardManager) appContext.getSystemService(Context.KEYGUARD_SERVICE))
                .inKeyguardRestrictedInputMode();
    }

    /*
     * The default behavior here is to open in a new tab.  If this is changed, ensure
     * intents with action NDEF_DISCOVERED (links beamed over NFC) are handled properly.
     */
    private @TabOpenType int getTabOpenType(Intent intent) {
        if (IntentUtils.safeGetBooleanExtra(
                    intent, ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false)) {
            return TabOpenType.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB;
        }
        if (IntentUtils.safeGetBooleanExtra(intent, EXTRA_OPEN_NEW_INCOGNITO_TAB, false)) {
            return TabOpenType.OPEN_NEW_INCOGNITO_TAB;
        }
        if (IntentUtils.safeGetIntExtra(
                    intent, TabOpenType.BRING_TAB_TO_FRONT_STRING, Tab.INVALID_TAB_ID)
                != Tab.INVALID_TAB_ID) {
            return TabOpenType.BRING_TAB_TO_FRONT;
        }

        String appId = IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID);
        // Due to users complaints, we are NOT reusing tabs for apps that do not specify an appId.
        if (appId == null
                || IntentUtils.safeGetBooleanExtra(intent, Browser.EXTRA_CREATE_NEW_TAB, false)) {
            return TabOpenType.OPEN_NEW_TAB;
        }

        // Intents from chrome open in the same tab by default, all others only clobber
        // tabs created by the same app.
        return mPackageName.equals(appId) ? TabOpenType.CLOBBER_CURRENT_TAB
                                          : TabOpenType.REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB;
    }

    private boolean isInvalidScheme(String scheme) {
        return scheme != null
            && (scheme.toLowerCase(Locale.US).equals(UrlConstants.JAVASCRIPT_SCHEME)
                || scheme.toLowerCase(Locale.US).equals(UrlConstants.JAR_SCHEME));
    }

    /**
     * Parses the scheme out of the URL if possible, trimming and getting rid of unsafe characters.
     * This is useful for determining if a URL has a sneaky, unsafe scheme, e.g. "java  script" or
     * "j$a$r". See: http://crbug.com/248398
     * @return The sanitized URL scheme or null if no scheme is specified.
     */
    private static String getSanitizedUrlScheme(String url) {
        if (url == null) {
            return null;
        }

        int colonIdx = url.indexOf(":");
        if (colonIdx < 0) {
            // No scheme specified for the url
            return null;
        }

        String scheme = url.substring(0, colonIdx).toLowerCase(Locale.US).trim();

        // Check for the presence of and get rid of all non-alphanumeric characters in the scheme,
        // except dash, plus and period. Those are the only valid scheme chars:
        // https://tools.ietf.org/html/rfc3986#section-3.1
        boolean nonAlphaNum = false;
        for (char ch : scheme.toCharArray()) {
            if (!Character.isLetterOrDigit(ch) && ch != '-' && ch != '+' && ch != '.') {
                nonAlphaNum = true;
                break;
            }
        }

        if (nonAlphaNum) {
            scheme = scheme.replaceAll("[^a-z0-9.+-]", "");
        }
        return scheme;
    }

    private boolean isJavascriptSchemeOrInvalidUrl(String url) {
        String urlScheme = getSanitizedUrlScheme(url);
        return isInvalidScheme(urlScheme);
    }

    /**
     * Retrieve the URL from the Intent, which may be in multiple locations.
     * If the URL is googlechrome:// scheme, parse the actual navigation URL.
     * @param intent Intent to examine.
     * @return URL from the Intent, or null if a valid URL couldn't be found.
     */
    public static String getUrlFromIntent(Intent intent) {
        String url = extractUrlFromIntent(intent);
        if (isGoogleChromeScheme(url)) {
            url = getUrlFromGoogleChromeSchemeUrl(url);
        }
        return url;
    }

    /**
     * Helper method to extract the raw URL from the intent, without further processing.
     * The URL may be in multiple locations.
     * @param intent Intent to examine.
     * @return Raw URL from the intent, or null if raw URL could't be found.
     */
    private static String extractUrlFromIntent(Intent intent) {
        if (intent == null) return null;
        String url = getUrlFromVoiceSearchResult(intent);
        if (url == null) url = ActivityDelegate.getInitialUrlForDocument(intent);
        if (url == null) url = getUrlForCustomTab(intent);
        if (url == null) url = intent.getDataString();
        if (url == null) return null;
        url = url.trim();
        return TextUtils.isEmpty(url) ? null : url;
    }

    private static String getUrlForCustomTab(Intent intent) {
        if (intent == null || intent.getData() == null) return null;
        Uri data = intent.getData();
        return TextUtils.equals(data.getScheme(), UrlConstants.CUSTOM_TAB_SCHEME)
                ? data.getQuery() : null;
    }

    @VisibleForTesting
    static String maybeAddAdditionalExtraHeaders(Intent intent, String url, String extraHeaders) {
        // For some apps, ContentResolver.getType(contentUri) returns "application/octet-stream",
        // instead of the registered MIME type when opening a document from Downloads. To work
        // around this, we pass the intent type in extra headers such that content request job can
        // get it.
        if (intent == null || url == null) return extraHeaders;

        String scheme = getSanitizedUrlScheme(url);
        if (!TextUtils.equals(scheme, UrlConstants.CONTENT_SCHEME)) return extraHeaders;

        String type = intent.getType();
        if (type == null || type.isEmpty()) return extraHeaders;

        // Only override the type for MHTML related types, which some applications get wrong.
        if (!isMhtmlMimeType(type)) return extraHeaders;

        String typeHeader = "X-Chrome-intent-type: " + type;
        return (extraHeaders == null) ? typeHeader : (extraHeaders + "\n" + typeHeader);
    }

    /** Return true if the type is one of the Mime types used for MHTML */
    static boolean isMhtmlMimeType(String type) {
        return type.equals("multipart/related") || type.equals("message/rfc822");
    }

    /**
     * @param intent An Intent to be checked.
     * @return Whether the intent has an file:// or content:// URL with MHTML MIME type.
     */
    @VisibleForTesting
    static boolean isIntentForMhtmlFileOrContent(Intent intent) {
        String url = getUrlFromIntent(intent);
        if (url == null) return false;
        String scheme = getSanitizedUrlScheme(url);
        boolean isContentUriScheme = TextUtils.equals(scheme, UrlConstants.CONTENT_SCHEME);
        boolean isFileUriScheme = TextUtils.equals(scheme, UrlConstants.FILE_SCHEME);
        if (!isContentUriScheme && !isFileUriScheme) return false;
        String type = intent.getType();
        if (type != null && isMhtmlMimeType(type)) {
            return true;
        }
        // Note that "application/octet-stream" type may be passed by some apps that do not know
        // about MHTML file types.
        if (!isFileUriScheme
                || (!TextUtils.isEmpty(type) && !type.equals("application/octet-stream"))) {
            return false;
        }

        // Get the file extension. We can't use MimeTypeMap.getFileExtensionFromUrl because it will
        // reject urls with characters that are valid in filenames (such as "!").
        String extension = FileUtils.getExtension(url);

        return extension.equals("mhtml") || extension.equals("mht");
    }

    /**
     * Adjusts the URL to account for the googlechrome:// scheme.
     * Currently, its only use is to handle navigations, only http and https URL is allowed.
     * @param url URL to be processed
     * @return The string with the scheme and prefixes chopped off, if a valid prefix was used.
     *         Otherwise returns null.
     */
    public static String getUrlFromGoogleChromeSchemeUrl(String url) {
        if (url.toLowerCase(Locale.US).startsWith(GOOGLECHROME_NAVIGATE_PREFIX)) {
            String parsedUrl = url.substring(GOOGLECHROME_NAVIGATE_PREFIX.length());
            if (!TextUtils.isEmpty(parsedUrl)) {
                String scheme = getSanitizedUrlScheme(parsedUrl);
                if (scheme == null) {
                    // If no scheme, assuming this is an http url.
                    parsedUrl = UrlConstants.HTTP_URL_PREFIX + parsedUrl;
                }
            }
            if (UrlUtilities.isHttpOrHttps(parsedUrl)) return parsedUrl;
        }

        return null;
    }

    /**
     * @param url URL to be tested
     * @return Whether the given URL adheres to the googlechrome:// scheme definition.
     */
    public static boolean isGoogleChromeScheme(String url) {
        if (url == null) return false;
        String urlScheme = Uri.parse(url).getScheme();
        return urlScheme != null && urlScheme.equals(GOOGLECHROME_SCHEME);
    }

    // TODO(mariakhomenko): pending referrer and pending incognito intent could potentially
    // not work correctly in multi-window. Store per-window information instead.

    /**
     * Records a pending referrer URL that we may be sending to ourselves through an intent.
     * @param intent The intent to which we add a referrer.
     * @param url The referrer URL.
     */
    public static void setPendingReferrer(Intent intent, String url) {
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(url));
        intent.putExtra(IntentHandler.EXTRA_REFERRER_ID, ++sReferrerId);
        sPendingReferrer = new Pair<Integer, String>(sReferrerId, url);
    }

    /**
     * Clears any pending referrer data.
     */
    public static void clearPendingReferrer() {
        sPendingReferrer = null;
    }

    /**
     * Retrieves pending referrer URL based on the given id.
     * @param id The referrer id.
     * @return The URL for the referrer or null if none found.
     */
    public static String getPendingReferrerUrl(int id) {
        if (sPendingReferrer != null && (sPendingReferrer.first == id)) {
            return sPendingReferrer.second;
        }
        return null;
    }

    /**
     * Keeps track of pending incognito URL to be loaded and ensures we allow to load it if it
     * comes back to us. This is a method for dispatching incognito URL intents from Chrome that
     * may or may not end up in Chrome.
     * @param intent The intent that will be sent.
     */
    public static void setPendingIncognitoUrl(Intent intent) {
        if (intent.getData() != null) {
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            sPendingIncognitoUrl = intent.getDataString();
        }
    }

    /**
     * Clears the pending incognito URL.
     */
    public static void clearPendingIncognitoUrl() {
        sPendingIncognitoUrl = null;
    }

    /**
     * @return Pending incognito URL that is allowed to be loaded without system token.
     */
    public static String getPendingIncognitoUrl() {
        return sPendingIncognitoUrl;
    }

    /**
     * Some applications may request to load the URL with a particular transition type.
     * @param intent Intent causing the URL load, may be null.
     * @param defaultTransition The transition to return if none specified in the intent.
     * @return The transition type to use for loading the URL.
     */
    public static int getTransitionTypeFromIntent(Intent intent, int defaultTransition) {
        if (intent == null) return defaultTransition;
        int transitionType = IntentUtils.safeGetIntExtra(
                intent, IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PageTransition.LINK);
        if (transitionType == PageTransition.TYPED) {
            return transitionType;
        } else if (transitionType != PageTransition.LINK
                && notSecureIsIntentChromeOrFirstParty(intent)) {
            // 1st party applications may specify any transition type.
            return transitionType;
        }
        return defaultTransition;
    }

    /**
     * Sets the launch type in a tab creation intent.
     * @param intent The Intent to be set.
     */
    public static void setTabLaunchType(Intent intent, @TabLaunchType int type) {
        intent.putExtra(EXTRA_TAB_LAUNCH_TYPE, type);
    }

    /**
     * @param intent An Intent to be checked.
     * @return The launch type of the tab to be created.
     */
    public static @Nullable @TabLaunchType Integer getTabLaunchType(Intent intent) {
        return IntentUtils.safeGetSerializableExtra(intent, EXTRA_TAB_LAUNCH_TYPE);
    }
}
