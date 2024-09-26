// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;

import android.app.Activity;
import android.app.KeyguardManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.Browser;
import android.speech.RecognizerResultsIntent;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalnav.IntentWithRequestMetadataHandler;
import org.chromium.chrome.browser.externalnav.IntentWithRequestMetadataHandler.RequestMetadata;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAUtils;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.renderer_host.ChromeNavigationUIData;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.content_public.common.Referrer;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.net.HttpUtil;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Handles all browser-related Intents. */
@JNINamespace("chrome::android")
public class IntentHandler {
    private static final String TAG = "IntentHandler";

    /** Tab ID to use when creating a new Tab. */
    private static final String EXTRA_TAB_ID = "com.android.chrome.tab_id";

    /** The tab id of the parent tab, if any. */
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

    /** Transition type is only set internally by a first-party app and has to be signed. */
    public static final String EXTRA_PAGE_TRANSITION_TYPE = "com.google.chrome.transition_type";

    /** Transition bookmark id is only set internally by a first-party app and has to be signed. */
    public static final String EXTRA_PAGE_TRANSITION_BOOKMARK_ID =
            "com.google.chrome.transition_bookmark_id";

    /** The original intent of the given intent before it was modified. */
    public static final String EXTRA_ORIGINAL_INTENT = "com.android.chrome.original_intent";

    /**
     * An extra to indicate that a particular intent was triggered from the first run experience
     * flow.
     */
    public static final String EXTRA_INVOKED_FROM_FRE = "com.android.chrome.invoked_from_fre";

    /** An extra to indicate that the intent was triggered from a launcher shortcut. */
    public static final String EXTRA_INVOKED_FROM_SHORTCUT =
            "com.android.chrome.invoked_from_shortcut";

    /** An extra to indicate that the intent was triggered from an app widget. */
    public static final String EXTRA_INVOKED_FROM_APP_WIDGET =
            "com.android.chrome.invoked_from_app_widget";

    /**
     * An extra to indicate that the intent was triggered by the launch new incognito tab feature.
     * See {@link org.chromium.chrome.browser.incognito.IncognitoTabLauncher}.
     */
    public static final String EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB =
            "org.chromium.chrome.browser.incognito.invoked_from_launch_new_incognito_tab";

    /** Intent extra used to deliver the original activity referrer. */
    public static final String EXTRA_ACTIVITY_REFERRER =
            "org.chromium.chrome.browser.activity_referrer";

    /** Intent extra used to deliver the package name of original #getCallingActivity if present. */
    public static final String EXTRA_CALLING_ACTIVITY_PACKAGE =
            "org.chromium.chrome.browser.calling_activity_package";

    /** Intent extra used to deliver the package name provided via #getLaunchedFromPackage. */
    public static final String EXTRA_LAUNCHED_FROM_PACKAGE =
            "org.chromium.chrome.browser.launched_from_package";

    /** A referrer id used for Chrome to Chrome referrer passing. */
    public static final String EXTRA_REFERRER_ID = "org.chromium.chrome.browser.referrer_id";

    /**
     * An extra for identifying the referrer policy to be used.
     * TODO(yusufo): Move this to support library.
     */
    public static final String EXTRA_REFERRER_POLICY =
            "android.support.browser.extra.referrer_policy";

    /**
     * Extra specifying additional urls that should each be opened in a new tab. If
     * EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP is present and true, these will be opened in a tab
     * group.
     */
    public static final String EXTRA_ADDITIONAL_URLS =
            "org.chromium.chrome.browser.additional_urls";

    /**
     * Extra specifying that additional urls opened should be part of a tab group parented to the
     * root url of the intent. Only valid if EXTRA_ADDITIONAL_URLS is present.
     */
    public static final String EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP =
            "org.chromium.chrome.browser.open_additional_urls_in_tab_group";

    /** Extra specifying to show regular overview mode. */
    public static final String EXTRA_OPEN_REGULAR_OVERVIEW_MODE =
            "org.chromium.chrome.browser.open_regular_overview_mode";

    /**
     * For multi-window, passes the id of the window. On Android S, this is synonymous with
     * the id of 'activity instance' among multiple instances that can be chosen on instance
     * switcher UI, ranging from 0 ~ max_instances - 1. -1 for an invalid id.
     */
    public static final String EXTRA_WINDOW_ID = "org.chromium.chrome.browser.window_id";

    /** Extra to indicate the launch type of the tab to be created. */
    private static final String EXTRA_TAB_LAUNCH_TYPE =
            "org.chromium.chrome.browser.tab_launch_type";

    /** A hash code for the URL to verify intent data hasn't been modified. */
    public static final String EXTRA_DATA_HASH_CODE = "org.chromium.chrome.browser.data_hash";

    /** A boolean to indicate whether incognito mode is currently selected. */
    public static final String EXTRA_INCOGNITO_MODE = "org.chromium.chrome.browser.incognito_mode";

    /** Byte array for the POST data when load a url, only Intents sent by Chrome can use this. */
    public static final String EXTRA_POST_DATA = "com.android.chrome.post_data";

    /**
     * The type of the POST data, need to be added to the HTTP request header, only Intents sent by
     * Chrome can use this.
     */
    public static final String EXTRA_POST_DATA_TYPE = "com.android.chrome.post_data_type";

    /**
     * A boolean to indicate whether this Intent originated from the Open In Browser Custom Tab
     * feature.
     */
    public static final String EXTRA_FROM_OPEN_IN_BROWSER =
            "com.android.chrome.from_open_in_browser";

    /**
     * A boolean to indicate that the Intent prefer a fresh new Chrome instance, not with tabs
     * from one of the existing disk files.
     */
    public static final String EXTRA_PREFER_NEW = "com.android.chrome.prefer_new";

    /**
     * Interested entities within Chrome relying on launching Incognito CCT should set this in their
     *{@link CustomTabIntent} in order to identify themselves for metric purposes.
     **/
    public static final String EXTRA_INCOGNITO_CCT_CALLER_ID =
            "org.chromium.chrome.browser.customtabs.EXTRA_INCOGNITO_CCT_CALLER_ID";

    /**
     * A boolean to indicate whether the ChromeTabbedActivity task was started by this Intent. Only
     * used for external View intents.
     */
    public static final String EXTRA_STARTED_TABBED_CHROME_TASK =
            "org.chromium.chrome.browser.started_chrome_task";

    /**
     * An ID of the FedCM invocation associated with this intent. It is used to keep a mapping from
     * IDs to openers, so that a CCT opened as a result of the FedCM API may send notifications to
     * the opener.
     */
    public static final String EXTRA_FEDCM_ID = "org.chromium.chrome.browser.fedcm_id";

    /**
     * A position of the new tab added to the tabs toolbar. Used when a tab is being moved from one
     * instance of the Chrome to another.
     */
    public static final String EXTRA_TAB_INDEX = "com.android.chrome.tab_index";

    /** A boolean to indicate whether an intent was launched via ChromeLauncherActivity. */
    public static final String EXTRA_LAUNCHED_VIA_CHROME_LAUNCHER_ACTIVITY =
            "org.chromium.chrome.browser.launched_via_chrome_launcher_activity";

    /** An enum to indicate whether the intent is created by link or tab. */
    public static final String EXTRA_URL_DRAG_SOURCE =
            "org.chromium.chrome.browser.url_drag_source";

    /** The id of a dragged tab that attempts to launch the intent. */
    public static final String EXTRA_DRAGGED_TAB_ID = "org.chromium.chrome.browser.dragdrop.tab_id";

    /** A boolean to indicate whether the intent should launch the history page in Chrome. */
    public static final String EXTRA_OPEN_HISTORY = "org.chromium.chrome.browser.open_history";

    /** A boolean to indicate whether the intent should launch only app specific history */
    public static final String EXTRA_APP_SPECIFIC_HISTORY =
            "org.chromium.chrome.browser.app_specific_history";

    /** The package name for the Google Search App. */
    public static final String PACKAGE_GSA = GSAUtils.GSA_PACKAGE_NAME;

    private static Pair<Integer, String> sPendingReferrer;
    private static int sReferrerId;
    private static String sPendingIncognitoUrl;

    private static final String PACKAGE_GMAIL = "com.google.android.gm";
    private static final String PACKAGE_PLUS = "com.google.android.apps.plus";
    private static final String PACKAGE_HANGOUTS = "com.google.android.talk";
    private static final String PACKAGE_MESSENGER = "com.google.android.apps.messaging";
    private static final String PACKAGE_YOUTUBE = "com.google.android.youtube";
    private static final String PACKAGE_LINE = "jp.naver.line.android";
    private static final String PACKAGE_WHATSAPP = "com.whatsapp";
    private static final String PACKAGE_YAHOO_MAIL = "com.yahoo.mobile.client.android.mail";
    private static final String PACKAGE_VIBER = "com.viber.voip";
    private static final String FACEBOOK_REFERRER_URL = "android-app://m.facebook.com";
    private static final String FACEBOOK_INTERNAL_BROWSER_REFERRER = "http://m.facebook.com";
    private static final String TWITTER_LINK_PREFIX = "http://t.co/";
    private static final String NEWS_LINK_PREFIX = "http://news.google.com/news/url?";
    private static final String YOUTUBE_LINK_PREFIX_HTTPS = "https://www.youtube.com/redirect?";
    private static final String YOUTUBE_LINK_PREFIX_HTTP = "http://www.youtube.com/redirect?";
    private static final String BRING_TAB_TO_FRONT_EXTRA = "BRING_TAB_TO_FRONT";
    public static final String BRING_TAB_TO_FRONT_SOURCE_EXTRA = "BRING_TAB_TO_FRONT_SOURCE";
    public static final String DAYDREAM_CATEGORY = "com.google.intent.category.DAYDREAM";
    public static final String SHARE_INTENT_HISTOGRAM = "Android.Intent.ShareIntentUrlCount";

    /**
     * Represents popular external applications that can load a page in Chrome via intent. DO NOT
     * reorder items in this interface, because it's mirrored to UMA (as ClientAppId). Values should
     * be enumerated from 0 and can't have gaps. When removing items, comment them out and keep
     * existing numeric values stable.
     */
    @IntDef({
        ExternalAppId.OTHER,
        ExternalAppId.GMAIL,
        ExternalAppId.FACEBOOK,
        ExternalAppId.PLUS,
        ExternalAppId.TWITTER,
        ExternalAppId.CHROME,
        ExternalAppId.HANGOUTS,
        ExternalAppId.MESSENGER,
        ExternalAppId.NEWS,
        ExternalAppId.LINE,
        ExternalAppId.WHATSAPP,
        ExternalAppId.GSA,
        ExternalAppId.WEBAPK,
        ExternalAppId.YAHOO_MAIL,
        ExternalAppId.VIBER,
        ExternalAppId.YOUTUBE,
        ExternalAppId.CAMERA,
        ExternalAppId.NUM_ENTRIES
    })
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
        int YOUTUBE = 15;
        int CAMERA = 16;
        // Update ClientAppId in enums.xml when adding new items.
        int NUM_ENTRIES = 17;
    }

    /**
     * Represents apps that launch Incognito CCT. DO NOT reorder items in this interface, because
     * it's mirrored to UMA (as {@link IncognitoCCTCallerId}). Values should be enumerated from 0.
     * When removing items, comment them out and keep existing numeric values stable.
     */
    @IntDef({
        IncognitoCCTCallerId.OTHER_APPS,
        IncognitoCCTCallerId.GOOGLE_APPS,
        IncognitoCCTCallerId.OTHER_CHROME_FEATURES,
        IncognitoCCTCallerId.READER_MODE,
        IncognitoCCTCallerId.READ_LATER,
        IncognitoCCTCallerId.EPHEMERAL_TAB,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface IncognitoCCTCallerId {
        int OTHER_APPS = 0;
        int GOOGLE_APPS = 1;
        // This should not be used, it's a fallback for Chrome features that didn't identify
        // themselves. Please see {@link
        // IncognitoCustomTabIntentDataProvider#addIncognitoExtrasForChromeFeatures}
        int OTHER_CHROME_FEATURES = 2;

        // Chrome Features
        int READER_MODE = 3;
        int READ_LATER = 4;

        // An ephemeral custom tab without incognito branding.
        int EPHEMERAL_TAB = 5;

        // Update {@link IncognitoCCTCallerId} in enums.xml when adding new items.
        int NUM_ENTRIES = 6;
    }

    /** Intent extra to open an incognito tab. */
    public static final String EXTRA_OPEN_NEW_INCOGNITO_TAB =
            "com.google.android.apps.chrome.EXTRA_OPEN_NEW_INCOGNITO_TAB";

    /** Intent extra to enable ephemeral browsing within the Custom Tab. */
    public static final String EXTRA_ENABLE_EPHEMERAL_BROWSING =
            "androidx.browser.customtabs.extra.ENABLE_EPHEMERAL_BROWSING";

    /** Scheme used by web pages to start up Chrome without an explicit Intent. */
    public static final String GOOGLECHROME_SCHEME = "googlechrome";

    private static boolean sTestIntentsEnabled;

    @IntDef({
        TabOpenType.OPEN_NEW_TAB,
        TabOpenType.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB,
        TabOpenType.REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB,
        TabOpenType.CLOBBER_CURRENT_TAB,
        TabOpenType.BRING_TAB_TO_FRONT,
        TabOpenType.OPEN_NEW_INCOGNITO_TAB,
        TabOpenType.REUSE_TAB_MATCHING_ID_ELSE_NEW_TAB
    })
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
        // Tab is reused only if the tab ID exists (tab ID is specified with the integer extra
        // REUSE_TAB_MATCHING_ID_STRING), and if the tab matches either the requested URL, or
        // the URL provided in the REUSE_TAB_ORIGINAL_URL_STRING extra.
        // Otherwise, the URL is opened in a new tab. REUSE_TAB_ORIGINAL_URL_STRING can be used if
        // the intent url is a result of a redirect, so that a tab pointing at the original URL can
        // be reused.
        int REUSE_TAB_MATCHING_ID_ELSE_NEW_TAB = 6;

        String REUSE_TAB_MATCHING_ID_STRING = "REUSE_TAB_MATCHING_ID";
        String REUSE_TAB_ORIGINAL_URL_STRING = "REUSE_TAB_ORIGINAL_URL";
    }

    @IntDef({
        BringToFrontSource.ACTIVATE_TAB,
        BringToFrontSource.NOTIFICATION,
        BringToFrontSource.SEARCH_ACTIVITY
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BringToFrontSource {
        int INVALID = -1;
        int ACTIVATE_TAB = 0;
        int NOTIFICATION = 1;
        int SEARCH_ACTIVITY = 2;
    }

    /** Sets whether or not test intents are enabled. */
    @VisibleForTesting
    public static void setTestIntentsEnabled(boolean enabled) {
        sTestIntentsEnabled = enabled;
    }

    private IntentHandler() {}

    /**
     * Returns information on the activity referrer. Queries the intent in case that the activity
     * was launched through the Chrome launcher activity, falling back to {@link
     * Activity#getReferrer()}.
     */
    public static String getActivityReferrer(Intent intent, @NonNull Activity activity) {
        String activityReferrer =
                IntentUtils.safeGetStringExtra(intent, IntentHandler.EXTRA_ACTIVITY_REFERRER);
        if (activityReferrer != null) {
            return activityReferrer;
        }
        Uri activityReferrerUri = activity.getReferrer();
        return (activityReferrerUri != null) ? activityReferrerUri.toString() : null;
    }

    /** Determines if Chrome was used to fire this intent. */
    public static boolean isExternalIntentSourceChrome(Intent intent) {
        // Intent should be sufficient for determining whether the intent originated from Chrome.
        return determineExternalIntentSource(intent, /* activity= */ null) == ExternalAppId.CHROME;
    }

    /**
     * Determines what App was used to fire this Intent.
     *
     * @param intent Intent that was used to launch Chrome.
     * @param activity Queried if the app which launched Chrome could not be determined from the
     *     intent.
     * @return ExternalAppId representing the app.
     */
    public static @ExternalAppId int determineExternalIntentSource(
            Intent intent, @Nullable Activity activity) {
        if (wasIntentSenderChrome(intent)) return ExternalAppId.CHROME;

        String appId = IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID);
        @ExternalAppId int externalId = ExternalAppId.OTHER;
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
            } else if (url != null
                    && (url.startsWith(YOUTUBE_LINK_PREFIX_HTTPS)
                            || url.startsWith(YOUTUBE_LINK_PREFIX_HTTP))) {
                externalId = ExternalAppId.YOUTUBE;
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

        if (externalId == ExternalAppId.OTHER && activity != null) {
            String activityReferrer = getActivityReferrer(intent, activity);
            if (activityReferrer != null
                    && activityReferrer.toLowerCase(Locale.getDefault()).endsWith("camera")) {
                return ExternalAppId.CAMERA;
            }
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
        } else if (packageName.equals(PACKAGE_YOUTUBE)) {
            return ExternalAppId.YOUTUBE;
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

    /**
     * Extracts referrer Uri from intent, if supplied.
     * @param intent The intent to use.
     * @return The referrer Uri.
     */
    private static Uri getReferrer(Intent intent) {
        Uri referrer = IntentUtils.safeGetParcelableExtra(intent, Intent.EXTRA_REFERRER);
        if (referrer != null) {
            String pendingReferrer =
                    IntentHandler.getPendingReferrerUrl(
                            IntentUtils.safeGetIntExtra(intent, EXTRA_REFERRER_ID, 0));
            return TextUtils.isEmpty(pendingReferrer) ? referrer : Uri.parse(pendingReferrer);
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
        CustomTabsSessionToken customTabsSession =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        if (referrerExtra == null && customTabsSession != null) {
            Referrer referrer =
                    CustomTabsConnection.getInstance()
                            .getDefaultReferrerForSession(customTabsSession);
            if (referrer != null) {
                referrerExtra = Uri.parse(referrer.getUrl());
            }
        }

        if (referrerExtra == null) return null;
        if (isValidReferrerHeader(referrerExtra)) {
            return referrerExtra.toString();
        } else if (IntentHandler.notSecureIsIntentChromeOrFirstParty(intent)
                || ChromeApplicationImpl.getComponent()
                        .resolveSessionDataHolder()
                        .canActiveHandlerUseReferrer(customTabsSession, referrerExtra)) {
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
        String headers = getExtraHeadersFromIntent(intent);
        if (headers != null) params.setVerbatimHeaders(headers);
    }

    public static int getReferrerPolicyFromIntent(Intent intent) {
        int policy =
                IntentUtils.safeGetIntExtra(intent, EXTRA_REFERRER_POLICY, ReferrerPolicy.DEFAULT);
        if (policy < ReferrerPolicy.MIN_VALUE || policy >= ReferrerPolicy.MAX_VALUE) {
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
        return TextUtils.equals(normalized.getScheme(), IntentUtils.ANDROID_APP_REFERRER_SCHEME)
                && !TextUtils.isEmpty(normalized.getHost());
    }

    /**
     * Constructs a valid referrer using the given authority.
     * @param authority The authority to use.
     * @return Referrer with default policy that uses the valid android app scheme, or null.
     */
    public static Referrer constructValidReferrerForAuthority(String authority) {
        if (TextUtils.isEmpty(authority)) return null;
        return new Referrer(
                new Uri.Builder()
                        .scheme(IntentUtils.ANDROID_APP_REFERRER_SCHEME)
                        .authority(authority)
                        .build()
                        .toString(),
                ReferrerPolicy.DEFAULT);
    }

    /**
     * Extracts the URL from voice search result intent.
     *
     * @return URL if it was found, null otherwise.
     */
    // TODO(crbug.com/40549331): Investigate whether this function can return a GURL instead,
    // or split into formatted/unformatted getUrl.
    static String getUrlFromVoiceSearchResult(Intent intent) {
        if (!RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS.equals(intent.getAction())) {
            return null;
        }
        ArrayList<String> results =
                IntentUtils.safeGetStringArrayListExtra(
                        intent, RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS);

        // Allow specifying a single voice result via the command line during testing (as the
        // 'am' command does not allow specifying an array of strings).
        if (results == null && sTestIntentsEnabled) {
            String testResult =
                    IntentUtils.safeGetStringExtra(
                            intent, RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS);
            if (testResult != null) {
                results = new ArrayList<String>();
                results.add(testResult);
            }
        }
        // The logic in this method should be moved to ChromeTabbedActivity eventually. We should
        // support async handling of voice search when native finishes initializing.
        if (results == null
                || results.size() == 0
                || !BrowserStartupController.getInstance().isFullBrowserStarted()) {
            return null;
        }
        String query = results.get(0);

        Profile profile = ProfileManager.getLastUsedRegularProfile();
        AutocompleteMatch match = AutocompleteCoordinator.classify(profile, query);

        if (!match.isSearchSuggestion()) return match.getUrl().getSpec();

        List<String> urls =
                IntentUtils.safeGetStringArrayListExtra(
                        intent, RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS);
        if (urls != null && urls.size() > 0) {
            return urls.get(0);
        } else {
            return TemplateUrlServiceFactory.getForProfile(profile)
                    .getUrlForVoiceSearchQuery(query)
                    .getSpec();
        }
    }

    /**
     * Start activity for the given trusted Intent.
     *
     * To make sure the intent is not dropped by Chrome, we send along an authentication token to
     * identify ourselves as a trusted sender. The method {@link #shouldIgnoreIntent} validates the
     * token.
     */
    public static void startActivityForTrustedIntent(Intent intent) {
        startActivityForTrustedIntentInternal(null, intent, null);
    }

    /**
     * Start activity for the given trusted Intent.
     *
     * To make sure the intent is not dropped by Chrome, we send along an authentication token to
     * identify ourselves as a trusted sender. The method {@link #shouldIgnoreIntent} validates the
     * token.
     */
    public static void startActivityForTrustedIntent(Context context, Intent intent) {
        startActivityForTrustedIntentInternal(context, intent, null);
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
        startActivityForTrustedIntentInternal(null, intent, ChromeLauncherActivity.class.getName());
    }

    private static void startActivityForTrustedIntentInternal(
            Context context, Intent intent, String componentClassName) {
        Context appContext = context == null ? ContextUtils.getApplicationContext() : context;
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
        IntentUtils.addTrustedIntentExtras(copiedIntent);
        appContext.startActivity(copiedIntent);
    }

    /**
     * Sets the Extra field 'EXTRA_HEADERS' on intent. If |extraHeaders| is empty or null,
     * removes 'EXTRA_HEADERS' from intent.
     *
     * @param extraHeaders   A map containing the set of headers. May be null.
     * @param intent         The intent to modify.
     */
    public static void setIntentExtraHeaders(
            @Nullable Map<String, String> extraHeaders, Intent intent) {
        if (extraHeaders == null || extraHeaders.isEmpty()) {
            intent.removeExtra(Browser.EXTRA_HEADERS);
        } else {
            Bundle bundle = new Bundle();
            for (Map.Entry<String, String> header : extraHeaders.entrySet()) {
                bundle.putString(header.getKey(), header.getValue());
            }
            intent.putExtra(Browser.EXTRA_HEADERS, bundle);
        }
    }

    /**
     * Returns a String (or null) containing the extra headers sent by the intent, if any.
     *
     * This methods skips the referrer header.
     *
     * @param intent The intent containing the bundle extra with the HTTP headers.
     */
    public static String getExtraHeadersFromIntent(Intent intent) {
        Bundle bundleExtraHeaders = IntentUtils.safeGetBundleExtra(intent, Browser.EXTRA_HEADERS);
        if (bundleExtraHeaders == null) return null;
        StringBuilder extraHeaders = new StringBuilder();

        boolean fromChrome = IntentHandler.wasIntentSenderChrome(intent);
        boolean shouldAllowNonSafelistedHeaders =
                CustomTabsConnection.getInstance().isFirstPartyOriginForIntent(intent);

        for (String key : bundleExtraHeaders.keySet()) {
            String value = bundleExtraHeaders.getString(key);

            if (!HttpUtil.isAllowedHeader(key, value)) {
                Log.w(TAG, "Ignoring forbidden header " + key + " in EXTRA_HEADERS.");
                continue;
            }

            // Strip the custom header that can only be added by ourselves.
            if ("x-chrome-intent-type".equals(key.toLowerCase(Locale.US))) continue;

            if (!fromChrome) {
                if (key.toLowerCase(Locale.US).startsWith("x-chrome-")) {
                    Log.w(TAG, "Ignoring x-chrome header " + key + " in EXTRA_HEADERS.");
                    continue;
                }

                if (!shouldAllowNonSafelistedHeaders
                        && !IntentHandlerJni.get().isCorsSafelistedHeader(key, value)) {
                    Log.w(TAG, "Ignoring non-CORS-safelisted header " + key + " in EXTRA_HEADERS.");
                    continue;
                }
            }

            if (extraHeaders.length() != 0) extraHeaders.append("\n");
            extraHeaders.append(key);
            extraHeaders.append(": ");
            extraHeaders.append(value);
        }

        return extraHeaders.length() == 0 ? null : extraHeaders.toString();
    }

    /**
     * Returns true if the app should ignore a given intent.
     *
     * @param intent Intent to check.
     * @param context the context to use for running screen-related checks.
     * @return true if the intent should be ignored.
     */
    public static boolean shouldIgnoreIntent(Intent intent, Context context) {
        return shouldIgnoreIntent(intent, context, /* isCustomTab= */ false);
    }

    /**
     * Returns true if the app should ignore a given intent.
     *
     * @param intent Intent to check.
     * @param isCustomTab True if the Intent will end up in a Custom Tab.
     * @return true if the intent should be ignored.
     */
    public static boolean shouldIgnoreIntent(Intent intent, boolean isCustomTab) {
        return shouldIgnoreIntent(intent, /* context= */ null, isCustomTab);
    }

    /**
     * Returns true if the app should ignore a given intent.
     *
     * @param intent Intent to check.
     * @param context the context to use for running screen-related checks.
     * @param isCustomTab True if the Intent will end up in a Custom Tab.
     * @return true if the intent should be ignored.
     */
    public static boolean shouldIgnoreIntent(Intent intent, Context context, boolean isCustomTab) {
        // Although not documented to, many/most methods that retrieve values from an Intent may
        // throw. Because we can't control what packages might send to us, we should catch any
        // Throwable and then fail closed (safe). This is ugly, but resolves top crashers in the
        // wild.
        try {
            // Ignore all invalid URLs, regardless of what the intent was.
            if (!intentHasValidUrl(intent)) {
                return true;
            }

            // Determine if this intent came from a trustworthy source (Chrome).
            boolean isFromChrome = wasIntentSenderChrome(intent);

            if (IntentUtils.safeGetBooleanExtra(intent, EXTRA_OPEN_NEW_INCOGNITO_TAB, false)
                    && !isAllowedIncognitoIntent(isFromChrome, isCustomTab, intent)) {
                return true;
            }

            // Ignore Daydream intents as these would cause us to re-navigate after the Device ON
            // flow. This can be removed once we migrate to the cardboard library.
            if (intent.hasCategory(DAYDREAM_CATEGORY)) return true;

            // Now if we have an empty URL and the intent was ACTION_MAIN,
            // we are pretty sure it is the launcher calling us to show up.
            // We can safely ignore the screen state.
            String url = getUrlFromIntent(intent);
            if (url == null && Intent.ACTION_MAIN.equals(intent.getAction())) {
                return false;
            }

            if (isFromChrome) return false;

            // Ignore all intents that specify a Chrome internal scheme if they did not come from
            // a trustworthy source.
            String scheme = ExternalNavigationHandler.getSanitizedUrlScheme(url);
            if (intentHasUnsafeInternalScheme(scheme, url, intent)) {
                Log.w(TAG, "Ignoring internal Chrome URL from untrustworthy source.");
                return true;
            }

            // Checking screen on/keyguard last as these calls can be slow.
            // If the screen is off, ignore any intents.
            if (!isScreenOn(context)) return true;
            if (ChromeFeatureList.sBlockIntentsWhileLocked.isEnabled() && isKeyguardLocked()) {
                return true;
            }
            return false;
        } catch (Throwable t) {
            return true;
        }
    }

    private static boolean isAllowedIncognitoIntent(
            boolean isChrome, boolean isCustomTab, Intent intent) {
        // "Open new incognito tab" is currently limited to Chrome for the Chrome app. It can be
        // launched by external apps if it's a Custom Tab, although there are additional checks in
        // IncognitoCustomTabIntentDataProvider#isValidIncognitoIntent.
        if (isChrome || isCustomTab) return true;

        // The pending incognito URL check is to handle the case where the user is shown an
        // Android intent picker while in incognito and they select the current Chrome instance
        // from the list.  In this case, we do not apply our Chrome token as the user has the
        // option to select apps outside of our control, so we rely on this in memory check
        // instead.
        String pendingUrl = getPendingIncognitoUrl();
        return pendingUrl != null && pendingUrl.equals(intent.getDataString());
    }

    private static boolean intentHasUnsafeInternalScheme(String scheme, String url, Intent intent) {
        if (scheme != null
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
                        || ContentUrlConstants.ABOUT_BLANK_URL.equals(lowerCaseUrl)
                        || UrlConstants.CHROME_DINO_URL.equals(lowerCaseUrl)) {
                    return false;
                }

                return true;
            }
        }
        return false;
    }

    @VisibleForTesting
    static boolean intentHasValidUrl(Intent intent) {
        String url = extractUrlFromIntent(intent);

        // Check if this is a valid googlechrome:// URL.
        if (isGoogleChromeScheme(url)) {
            url = ExternalNavigationHandler.getUrlFromSelfSchemeUrl(GOOGLECHROME_SCHEME, url);
            if (url == null) return false;
        }

        // Always drop insecure urls.
        if (url != null && isJavascriptSchemeOrInvalidUrl(url)) {
            return false;
        }

        return true;
    }

    /**
     * @param intent An Intent to be checked.
     * @return Whether an intent originates from Chrome.
     */
    public static boolean wasIntentSenderChrome(@Nullable Intent intent) {
        return IntentUtils.isTrustedIntentFromSelf(intent);
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

        if (IntentUtils.isTrustedIntentFromSelf(intent)) return true;

        // First-party Google apps re-use the secure application code extra for historical reasons.
        PendingIntent token =
                IntentUtils.safeGetParcelableExtra(
                        intent, IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        if (token == null) return false;
        if (ExternalAuthUtils.getInstance().isGoogleSigned(token.getCreatorPackage())) {
            return true;
        }
        return false;
    }

    private static boolean isScreenOn(Context context) {
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }

        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);

        return powerManager.isInteractive();
    }

    private static boolean isKeyguardLocked() {
        return ((KeyguardManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.KEYGUARD_SERVICE))
                .isKeyguardLocked();
    }

    /*
     * The default behavior here is to open in a new tab.  If this is changed, ensure
     * intents with action NDEF_DISCOVERED (links beamed over NFC) are handled properly.
     */
    public static @TabOpenType int getTabOpenType(Intent intent) {
        if (IntentUtils.safeGetBooleanExtra(
                intent, WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false)) {
            return TabOpenType.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB;
        }
        if (IntentUtils.safeGetBooleanExtra(intent, EXTRA_OPEN_NEW_INCOGNITO_TAB, false)) {
            return TabOpenType.OPEN_NEW_INCOGNITO_TAB;
        }
        if (getBringTabToFrontId(intent) != Tab.INVALID_TAB_ID) {
            return TabOpenType.BRING_TAB_TO_FRONT;
        }

        String appId = IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID);
        // Due to users complaints, we are NOT reusing tabs for apps that do not specify an appId.
        if (appId == null
                || IntentUtils.safeGetBooleanExtra(intent, Browser.EXTRA_CREATE_NEW_TAB, false)) {
            return TabOpenType.OPEN_NEW_TAB;
        }

        int tabId =
                IntentUtils.safeGetIntExtra(
                        intent, TabOpenType.REUSE_TAB_MATCHING_ID_STRING, Tab.INVALID_TAB_ID);
        if (tabId != Tab.INVALID_TAB_ID) {
            return TabOpenType.REUSE_TAB_MATCHING_ID_ELSE_NEW_TAB;
        }

        // Intents from chrome open in the same tab by default, all others only clobber
        // tabs created by the same app.
        return ContextUtils.getApplicationContext().getPackageName().equals(appId)
                ? TabOpenType.CLOBBER_CURRENT_TAB
                : TabOpenType.REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB;
    }

    private static boolean isInvalidScheme(String scheme) {
        return scheme != null
                && (scheme.toLowerCase(Locale.US).equals(UrlConstants.JAVASCRIPT_SCHEME)
                        || scheme.toLowerCase(Locale.US).equals(UrlConstants.JAR_SCHEME));
    }

    private static boolean isJavascriptSchemeOrInvalidUrl(String url) {
        String urlScheme = ExternalNavigationHandler.getSanitizedUrlScheme(url);
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
            url = ExternalNavigationHandler.getUrlFromSelfSchemeUrl(GOOGLECHROME_SCHEME, url);
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
        if (url == null) url = getUrlForCustomTab(intent);
        if (url == null) url = getUrlForWebapp(intent);
        if (url == null) url = getUrlFromShareIntent(intent);
        if (url == null) url = intent.getDataString();
        if (url == null) return null;
        url = url.trim();
        return TextUtils.isEmpty(url) ? null : url;
    }

    /**
     * Extracts all Strings terminated by whitespace with the specified prefix.
     *
     * @param text Text to examine.
     * @param prefix The prefix on which to extract Strings.
     * @param results The list to insert results into.
     */
    private static void extractStringsWithPrefix(String text, String prefix, List<String> results) {
        int i = 0;
        while (i < text.length()) {
            int startIndex = text.indexOf(prefix, i);
            if (startIndex == -1) return;
            for (i = startIndex + prefix.length(); i < text.length(); i++) {
                if (Character.isWhitespace(text.charAt(i))) {
                    results.add(text.substring(startIndex, i));
                    break;
                }
            }
            if (i >= text.length()) results.add(text.substring(startIndex));
        }
    }

    /**
     * Extract a raw URL from the Share intent text.
     *
     * <p>This first try to extract raw http/https URLs. In the case of multiple URLs being present,
     * picks the last one. If no explicit URLs are found, try using autocomplete to resembles the
     * URL. If not, it will construct a search query with the default search engine.
     *
     * @param intent Intent to examine.
     * @return URL from the intent, or null if no URL could be found.
     */
    public static @Nullable String getUrlFromShareIntent(Intent intent) {
        if (!Intent.ACTION_SEND.equals(intent.getAction())
                || !"text/plain".equals(intent.getType())) {
            return null;
        }

        String text = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_TEXT);
        List<String> urls = new ArrayList<>();
        if (!TextUtils.isEmpty(text)) {
            extractStringsWithPrefix(text, UrlConstants.HTTP_URL_PREFIX, urls);
            extractStringsWithPrefix(text, UrlConstants.HTTPS_URL_PREFIX, urls);
        }

        // Record a small exact linear histogram as we mostly care about 0/1/2, but the presence of
        // larger counts would be interesting.
        RecordHistogram.recordExactLinearHistogram(SHARE_INTENT_HISTOGRAM, urls.size(), 5);

        if (!urls.isEmpty()) {
            // If multiple URLs are present, somewhat arbitrarily pick the last one (preferring
            // https) - share actions seem to usually put the URL at the end.
            return urls.get(urls.size() - 1);
        }

        if (TextUtils.isEmpty(text)
                || !BrowserStartupController.getInstance().isFullBrowserStarted()) {
            return null;
        }

        Profile profile = ProfileManager.getLastUsedRegularProfile();
        AutocompleteMatch match = AutocompleteCoordinator.classify(profile, text);
        if (match != null) return match.getUrl().getSpec();

        return TemplateUrlServiceFactory.getForProfile(profile).getUrlForSearchQuery(text);
    }

    private static String getUrlForCustomTab(Intent intent) {
        if (intent == null || intent.getData() == null) return null;
        Uri data = intent.getData();
        return TextUtils.equals(data.getScheme(), UrlConstants.CUSTOM_TAB_SCHEME)
                ? data.getQuery()
                : null;
    }

    private static String getUrlForWebapp(Intent intent) {
        if (intent == null || intent.getData() == null) return null;
        Uri data = intent.getData();
        return TextUtils.equals(data.getScheme(), WebappActivity.WEBAPP_SCHEME)
                ? IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_URL)
                : null;
    }

    public static String maybeAddAdditionalContentHeaders(
            Intent intent, String url, String extraHeaders) {
        // For some apps, ContentResolver.getType(contentUri) returns "application/octet-stream",
        // instead of the registered MIME type when opening a document from Downloads. To work
        // around this, we pass the intent type in extra headers such that content request job can
        // get it.
        if (intent == null || url == null) return extraHeaders;

        String scheme = ExternalNavigationHandler.getSanitizedUrlScheme(url);
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
        String scheme = ExternalNavigationHandler.getSanitizedUrlScheme(url);
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
    public static void setPendingReferrer(Intent intent, GURL url) {
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(url.getSpec()));
        intent.putExtra(IntentHandler.EXTRA_REFERRER_ID, ++sReferrerId);
        sPendingReferrer = new Pair<Integer, String>(sReferrerId, url.getSpec());
    }

    /** Clears any pending referrer data. */
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

    /** Clears the pending incognito URL. */
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
        int transitionType =
                IntentUtils.safeGetIntExtra(
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
     * @return The launch type of the tab to be created. If null a reasonable default should be
     *     chosen downstream.
     */
    public static @Nullable @TabLaunchType Integer getTabLaunchType(Intent intent) {
        return IntentUtils.safeGetSerializableExtra(intent, EXTRA_TAB_LAUNCH_TYPE);
    }

    /**
     * Creates an Intent that will launch a ChromeTabbedActivity on the new tab page. The Intent
     * will be trusted and therefore able to launch Incognito tabs.
     * @param context A {@link Context} to access class and package information.
     * @param incognito Whether the tab should be opened in Incognito.
     * @return The {@link Intent} to launch.
     */
    public static Intent createTrustedOpenNewTabIntent(Context context, boolean incognito) {
        Intent newIntent = new Intent();
        newIntent.setAction(Intent.ACTION_VIEW);
        newIntent.setData(Uri.parse(UrlConstants.NTP_URL));
        newIntent.setClass(context, ChromeLauncherActivity.class);
        newIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        newIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        newIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, incognito);
        IntentUtils.addTrustedIntentExtras(newIntent);

        return newIntent;
    }

    /**
     * Creates an Intent that tells Chrome to bring an Activity for a particular Tab back to the
     * foreground.
     * @param tabId The id of the Tab to bring to the foreground.
     * @param bringToFrontSource The source of the bring to front Intent, used for gathering
     *         metrics.
     * @return Created Intent or null if this operation isn't possible.
     */
    public static @Nullable Intent createTrustedBringTabToFrontIntent(
            int tabId, @BringToFrontSource int bringToFrontSource) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, ChromeLauncherActivity.class);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(BRING_TAB_TO_FRONT_EXTRA, tabId);
        intent.putExtra(BRING_TAB_TO_FRONT_SOURCE_EXTRA, bringToFrontSource);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }

    public static int getBringTabToFrontId(Intent intent) {
        if (!wasIntentSenderChrome(intent)) return Tab.INVALID_TAB_ID;
        return IntentUtils.safeGetIntExtra(intent, BRING_TAB_TO_FRONT_EXTRA, Tab.INVALID_TAB_ID);
    }

    /** Sets the Tab Id extra for a given intent. Will only be usable by trusted Chrome intents. */
    public static void setTabId(Intent intent, int tabId) {
        intent.putExtra(IntentHandler.EXTRA_TAB_ID, tabId);
    }

    /**
     * @return the Tab Id extra from an intent, or INVALID_TAB_ID if Tab Id isn't present, or the
     * intent isn't trusted.
     */
    public static int getTabId(@Nullable Intent intent) {
        if (!wasIntentSenderChrome(intent)) return Tab.INVALID_TAB_ID;
        return IntentUtils.safeGetIntExtra(intent, EXTRA_TAB_ID, Tab.INVALID_TAB_ID);
    }

    /**
     * Handles an inconsistency in the Android platform, where if an Activity finishes itself, then
     * is resumed from recents, it's re-launched with the original intent that launched the activity
     * initially.
     *
     * @return the provided intent, if the intent is not from Android Recents. Otherwise, rewrites
     *         the intent to be a consistent MAIN intent from recents.
     */
    public static Intent rewriteFromHistoryIntent(Intent intent) {
        // When a self-finished Activity is created from recents, Android launches it with its
        // original base intent (with FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY added). This can lead
        // to duplicating actions when launched from recents, like re-launching tabs, or firing
        // additional app redirects, etc.
        // Instead of teaching all of Chrome about this, just make intents consistent when Chrome is
        // created from recents.
        if (0 != (intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY)) {
            Intent newIntent = new Intent(Intent.ACTION_MAIN);
            // Make sure to carry over the FROM_HISTORY flag to avoid confusing metrics.
            newIntent.setFlags(intent.getFlags());
            newIntent.addCategory(Intent.CATEGORY_LAUNCHER);
            newIntent.setComponent(intent.getComponent());
            newIntent.setPackage(intent.getPackage());
            return newIntent;
        }
        return intent;
    }

    /**
     * Bring the browser to foreground and switch to the tab.
     * @param tab Tab to switch to.
     */
    public static void bringTabToFront(Tab tab) {
        Intent newIntent =
                createTrustedBringTabToFrontIntent(tab.getId(), BringToFrontSource.SEARCH_ACTIVITY);
        if (newIntent != null) {
            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentUtils.safeStartActivity(ContextUtils.getApplicationContext(), newIntent);
        }
    }

    /** Create a LoadUrlParams for handling a VIEW intent. */
    public static LoadUrlParams createLoadUrlParamsForIntent(
            String url, Intent intent, long intentHandlingUptimeMillis) {
        var asyncTabParams =
                AsyncTabParamsManagerSingleton.getInstance()
                        .getAsyncTabParams()
                        .get(getTabId(intent));
        if (asyncTabParams != null && asyncTabParams.getLoadUrlParams() != null) {
            return asyncTabParams.getLoadUrlParams();
        }

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        RequestMetadata metadata =
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent);

        loadUrlParams.setIntentReceivedTimestamp(intentHandlingUptimeMillis);
        loadUrlParams.setHasUserGesture(metadata == null ? false : metadata.hasUserGesture());
        // Add FROM_API to ensure intent handling isn't used again. Without FROM_API Chrome could
        // get stuck in a loop continually being asked to open a link, and then calling out to the
        // system.
        int transitionType = PageTransition.LINK | PageTransition.FROM_API;
        loadUrlParams.setTransitionType(getTransitionTypeFromIntent(intent, transitionType));
        String referrer = getReferrerUrlIncludingExtraHeaders(intent);
        if (referrer != null) {
            loadUrlParams.setReferrer(
                    new Referrer(referrer, IntentHandler.getReferrerPolicyFromIntent(intent)));
        }

        String headers = getExtraHeadersFromIntent(intent);
        headers = maybeAddAdditionalContentHeaders(intent, url, headers);

        if (IntentHandler.wasIntentSenderChrome(intent)) {
            // Handle post data case.
            String postDataType =
                    IntentUtils.safeGetStringExtra(intent, IntentHandler.EXTRA_POST_DATA_TYPE);
            byte[] postData =
                    IntentUtils.safeGetByteArrayExtra(intent, IntentHandler.EXTRA_POST_DATA);
            if (!TextUtils.isEmpty(postDataType) && postData != null && postData.length != 0) {
                StringBuilder appendToHeader = new StringBuilder();
                appendToHeader.append("Content-Type: ");
                appendToHeader.append(postDataType);
                if (TextUtils.isEmpty(headers)) {
                    headers = appendToHeader.toString();
                } else {
                    headers = headers + "\r\n" + appendToHeader.toString();
                }

                loadUrlParams.setPostData(ResourceRequestBody.createFromBytes(postData));
            }

            // Attach bookmark id to the params if it's present in the intent.
            String bookmarkIdString =
                    IntentUtils.safeGetStringExtra(
                            intent, IntentHandler.EXTRA_PAGE_TRANSITION_BOOKMARK_ID);
            if (!TextUtils.isEmpty(bookmarkIdString)) {
                BookmarkId bookmarkId = BookmarkId.getBookmarkIdFromString(bookmarkIdString);
                ChromeNavigationUIData navData = new ChromeNavigationUIData();
                navData.setBookmarkId(
                        bookmarkId.getType() == BookmarkType.NORMAL ? bookmarkId.getId() : -1);
                loadUrlParams.setNavigationUIDataSupplier(navData::createUnownedNativeCopy);
            }
        } else {
            // Intent is not coming from Chrome, the sender can't be trusted.
            loadUrlParams.setInitiatorOrigin(Origin.createOpaqueOrigin());
        }
        loadUrlParams.setVerbatimHeaders(headers);
        loadUrlParams.setIsRendererInitiated(
                metadata == null ? false : metadata.isRendererInitiated());

        return loadUrlParams;
    }

    /**
     * Whether bundle has any extra that indicates an incognito tab will be launched.
     * @param extras A bundle that carries extras
     * @return True if there is any incognito related extra, otherwise return false.
     */
    public static boolean hasAnyIncognitoExtra(@Nullable Bundle extras) {
        if (extras == null) return false;
        return IntentUtils.safeGetBoolean(extras, EXTRA_INCOGNITO_MODE, false)
                || IntentUtils.safeGetBoolean(extras, EXTRA_OPEN_NEW_INCOGNITO_TAB, false)
                || IntentUtils.safeGetBoolean(
                        extras, EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false)
                || IntentUtils.safeGetBoolean(extras, EXTRA_ENABLE_EPHEMERAL_BROWSING, false);
    }

    @NativeMethods
    interface Natives {
        boolean isCorsSafelistedHeader(String name, String value);
    }
}
