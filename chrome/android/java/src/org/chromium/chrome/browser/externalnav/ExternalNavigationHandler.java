// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.EXTRA_BROWSER_LAUNCH_SOURCE;

import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.SystemClock;
import android.provider.Browser;
import android.support.annotation.IntDef;
import android.text.TextUtils;
import android.util.Pair;
import android.webkit.WebView;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.LaunchSourceType;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.webapps.WebappScopePolicy;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.PageTransition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Logic related to the URL overriding/intercepting functionality.
 * This feature allows Chrome to convert certain navigations to Android Intents allowing
 * applications like Youtube to direct users clicking on a http(s) link to their native app.
 */
public class ExternalNavigationHandler {
    private static final String TAG = "UrlHandler";

    // Enables debug logging on a local build.
    private static final boolean DEBUG = false;

    private static final String WTAI_URL_PREFIX = "wtai://wp/";
    private static final String WTAI_MC_URL_PREFIX = "wtai://wp/mc;";

    private static final String PLAY_PACKAGE_PARAM = "id";
    private static final String PLAY_REFERRER_PARAM = "referrer";
    private static final String PLAY_APP_PATH = "/store/apps/details";
    private static final String PLAY_HOSTNAME = "play.google.com";

    @VisibleForTesting
    static final String EXTRA_BROWSER_FALLBACK_URL = "browser_fallback_url";

    // An extra that may be specified on an intent:// URL that contains an encoded value for the
    // referrer field passed to the market:// URL in the case where the app is not present.
    @VisibleForTesting
    static final String EXTRA_MARKET_REFERRER = "market_referrer";

    // These values are persisted in histograms. Please do not renumber. Append only.
    @IntDef({AiaIntent.FALLBACK_USED, AiaIntent.SERP, AiaIntent.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AiaIntent {
        int FALLBACK_USED = 0;
        int SERP = 1;
        int OTHER = 2;

        int NUM_ENTRIES = 3;
    }

    private final ExternalNavigationDelegate mDelegate;

    /**
     * Result types for checking if we should override URL loading.
     * NOTE: this enum is used in UMA, do not reorder values. Changes should be append only.
     * Values should be numerated from 0 and can't have gaps.
     */
    @IntDef({OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
            OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB,
            OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION,
            OverrideUrlLoadingResult.NO_OVERRIDE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OverrideUrlLoadingResult {
        /* We should override the URL loading and launch an intent. */
        int OVERRIDE_WITH_EXTERNAL_INTENT = 0;
        /* We should override the URL loading and clobber the current tab. */
        int OVERRIDE_WITH_CLOBBERING_TAB = 1;
        /* We should override the URL loading.  The desired action will be determined
         * asynchronously (e.g. by requiring user confirmation). */
        int OVERRIDE_WITH_ASYNC_ACTION = 2;
        /* We shouldn't override the URL loading. */
        int NO_OVERRIDE = 3;

        int NUM_ENTRIES = 4;
    }

    /**
     * A constructor for UrlHandler.
     *
     * @param tab The tab that initiated the external intent.
     */
    public ExternalNavigationHandler(Tab tab) {
        this(new ExternalNavigationDelegateImpl(tab));
    }

    /**
     * Constructs a new instance of {@link ExternalNavigationHandler}, using the injected
     * {@link ExternalNavigationDelegate}.
     */
    public ExternalNavigationHandler(ExternalNavigationDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Determines whether the URL needs to be sent as an intent to the system,
     * and sends it, if appropriate.
     * @return Whether the URL generated an intent, caused a navigation in
     *         current tab, or wasn't handled at all.
     */
    public @OverrideUrlLoadingResult int shouldOverrideUrlLoading(ExternalNavigationParams params) {
        if (DEBUG) Log.i(TAG, "shouldOverrideUrlLoading called on " + params.getUrl());
        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(params.getUrl(), Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", params.getUrl(), ex);
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        boolean hasBrowserFallbackUrl = false;
        String browserFallbackUrl =
                IntentUtils.safeGetStringExtra(intent, EXTRA_BROWSER_FALLBACK_URL);
        if (browserFallbackUrl != null
                && UrlUtilities.isValidForIntentFallbackNavigation(browserFallbackUrl)) {
            hasBrowserFallbackUrl = true;
        } else {
            browserFallbackUrl = null;
        }

        long time = SystemClock.elapsedRealtime();
        @OverrideUrlLoadingResult
        int result = shouldOverrideUrlLoadingInternal(
                params, intent, hasBrowserFallbackUrl, browserFallbackUrl);
        RecordHistogram.recordTimesHistogram("Android.StrictMode.OverrideUrlLoadingTime",
                SystemClock.elapsedRealtime() - time, TimeUnit.MILLISECONDS);

        if (result != OverrideUrlLoadingResult.NO_OVERRIDE) {
            int pageTransitionCore = params.getPageTransition() & PageTransition.CORE_MASK;
            boolean isFormSubmit = pageTransitionCore == PageTransition.FORM_SUBMIT;
            boolean isRedirectFromFormSubmit = isFormSubmit && params.isRedirect();
            if (isRedirectFromFormSubmit) {
                RecordHistogram.recordBooleanHistogram(
                        "Android.Intent.LaunchExternalAppFormSubmitHasUserGesture",
                        params.hasUserGesture());
            }
        } else if (result == OverrideUrlLoadingResult.NO_OVERRIDE && hasBrowserFallbackUrl
                && (params.getRedirectHandler() == null
                        // For instance, if this is a chained fallback URL, we ignore it.
                        || !params.getRedirectHandler().shouldNotOverrideUrlLoading())) {
            if (InstantAppsHandler.isIntentToInstantApp(intent)) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.InstantApps.DirectInstantAppsIntent", AiaIntent.FALLBACK_USED,
                        AiaIntent.NUM_ENTRIES);
            }

            return clobberCurrentTabWithFallbackUrl(browserFallbackUrl, params);
        }
        return result;
    }

    private boolean resolversSubsetOf(List<ResolveInfo> infos, List<ResolveInfo> container) {
        if (container == null) return false;
        HashSet<ComponentName> containerSet = new HashSet<>();
        for (ResolveInfo info : container) {
            containerSet.add(
                    new ComponentName(info.activityInfo.packageName, info.activityInfo.name));
        }
        for (ResolveInfo info : infos) {
            if (!containerSet.contains(new ComponentName(
                    info.activityInfo.packageName, info.activityInfo.name))) {
                return false;
            }
        }
        return true;
    }

    private @OverrideUrlLoadingResult int shouldOverrideUrlLoadingInternal(
            ExternalNavigationParams params, Intent intent, boolean hasBrowserFallbackUrl,
            String browserFallbackUrl) {
        // http://crbug.com/441284 : Disallow firing external intent while Chrome is in the
        // background.
        if (params.isApplicationMustBeInForeground() && !mDelegate.isChromeAppInForeground()) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Chrome is not in foreground");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }
        // http://crbug.com/464669 : Disallow firing external intent from background tab.
        if (params.isBackgroundTabNavigation()) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Navigation in background tab");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // pageTransition is a combination of an enumeration (core value) and bitmask.
        int pageTransitionCore = params.getPageTransition() & PageTransition.CORE_MASK;
        boolean isLink = pageTransitionCore == PageTransition.LINK;
        boolean isFormSubmit = pageTransitionCore == PageTransition.FORM_SUBMIT;
        boolean isFromIntent = (params.getPageTransition() & PageTransition.FROM_API) != 0;
        boolean isForwardBackNavigation =
                (params.getPageTransition() & PageTransition.FORWARD_BACK) != 0;
        boolean isExternalProtocol = !UrlUtilities.isAcceptedScheme(params.getUrl());

        // http://crbug.com/169549 : If you type in a URL that then redirects in server side to an
        // link that cannot be rendered by the browser, we want to show the intent picker.
        boolean isTyped = (pageTransitionCore == PageTransition.TYPED)
                || ((params.getPageTransition() & PageTransition.FROM_ADDRESS_BAR) != 0);
        boolean typedRedirectToExternalProtocol = isTyped && params.isRedirect()
                && isExternalProtocol;

        // We do not want to show the intent picker for core types typed, bookmarks, auto toplevel,
        // generated, keyword, keyword generated. See below for exception to typed URL and
        // redirects:
        // - http://crbug.com/143118 : URL intercepting should not be invoked on navigations
        //   initiated by the user in the omnibox / NTP.
        // - http://crbug.com/159153 : Don't override http or https URLs from the NTP or bookmarks.
        // - http://crbug.com/162106: Intent picker should not be presented on returning to a page.
        //   This should be covered by not showing the picker if the core type is reload.

        // http://crbug.com/164194 . A navigation forwards or backwards should never trigger
        // the intent picker.
        if (isForwardBackNavigation) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Forward or back navigation");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // http://crbug.com/605302 : Allow Chrome to handle all pdf file downloads.
        if (!isExternalProtocol && mDelegate.isPdfDownload(params.getUrl())) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: PDF downloads are now handled by Chrome");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // If accessing a file URL, ensure that the user has granted the necessary file access
        // to Chrome.  This check should happen for reloads, navigations, etc..., which is why
        // it occurs before the subsequent blocks.
        if (params.getUrl().startsWith(UrlConstants.FILE_URL_SHORT_PREFIX)
                && mDelegate.shouldRequestFileAccess(params.getUrl())) {
            mDelegate.startFileIntent(intent, params.getReferrerUrl(),
                    params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent());
            if (DEBUG) Log.i(TAG, "OVERRIDE_WITH_ASYNC_ACTION: Requesting filesystem access");
            return OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION;
        }

        // http://crbug.com/149218: We want to show the intent picker for ordinary links, providing
        // the link is not an incoming intent from another application, unless it's a redirect (see
        // below).
        boolean linkNotFromIntent = isLink && !isFromIntent;

        boolean isOnEffectiveIntentRedirect = params.getRedirectHandler() == null ? false
                : params.getRedirectHandler().isOnEffectiveIntentRedirectChain();

        // http://crbug.com/170925: We need to show the intent picker when we receive an intent from
        // another app that 30x redirects to a YouTube/Google Maps/Play Store/Google+ URL etc.
        boolean incomingIntentRedirect = (isLink && isFromIntent && params.isRedirect())
                || isOnEffectiveIntentRedirect;


        // http://crbug/331571 : Do not override a navigation started from user typing.
        // http://crbug/424029 : Need to stay in Chrome for an intent heading explicitly to Chrome.
        // http://crbug/881740 : Relax stay in Chrome restriction for Custom Tabs.
        if (params.getRedirectHandler() != null) {
            TabRedirectHandler handler = params.getRedirectHandler();
            boolean shouldStayInChrome = handler.shouldStayInChrome(
                    isExternalProtocol, mDelegate.isIntentForTrustedCallingApp(intent));
            if (shouldStayInChrome || handler.shouldNotOverrideUrlLoading()) {
                // http://crbug.com/659301: Handle redirects to Instant Apps out of Custom Tabs.
                if (handler.isFromCustomTabIntent() && !isExternalProtocol && incomingIntentRedirect
                        && !handler.shouldNavigationTypeStayInChrome()
                        && mDelegate.maybeLaunchInstantApp(
                                   params.getUrl(), params.getReferrerUrl(), true)) {
                    if (DEBUG) {
                        Log.i(TAG, "OVERRIDE_WITH_EXTERNAL_INTENT: Launching redirect to "
                                + "an instant app");
                    }
                    return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
                }
                if (DEBUG) Log.i(TAG, "NO_OVERRIDE: RedirectHandler decision");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }
        }

        // http://crbug.com/181186: We need to show the intent picker when we receive a redirect
        // following a form submit.
        boolean isRedirectFromFormSubmit = isFormSubmit && params.isRedirect();

        if (!typedRedirectToExternalProtocol) {
            if (!linkNotFromIntent && !incomingIntentRedirect && !isRedirectFromFormSubmit) {
                if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Incoming intent (not a redirect)");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }
            if (params.getRedirectHandler() != null
                    && params.getRedirectHandler().isNavigationFromUserTyping()) {
                if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Navigation from user typing");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }
        }

        // Don't override navigation from a chrome:* url to http or https. For example,
        // when clicking a link in bookmarks or most visited. When navigating from such a
        // page, there is clear intent to complete the navigation in Chrome.
        if (params.getReferrerUrl() != null
                && params.getReferrerUrl().startsWith(UrlConstants.CHROME_URL_PREFIX)
                && (params.getUrl().startsWith(UrlConstants.HTTP_URL_PREFIX)
                        || params.getUrl().startsWith(UrlConstants.HTTPS_URL_PREFIX))) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Link from an internal chrome:// page");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (params.getUrl().startsWith(WTAI_MC_URL_PREFIX)) {
            // wtai://wp/mc;number
            // number=string(phone-number)
            mDelegate.startActivity(new Intent(Intent.ACTION_VIEW,
                    Uri.parse(WebView.SCHEME_TEL
                            + params.getUrl().substring(WTAI_MC_URL_PREFIX.length()))), false);
            if (DEBUG) Log.i(TAG, "OVERRIDE_WITH_EXTERNAL_INTENT wtai:// link handled");
            RecordUserAction.record("Android.PhoneIntent");
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }

        if (params.getUrl().startsWith(WTAI_URL_PREFIX)) {
            // TODO: handle other WTAI schemes.
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Unsupported wtai:// link");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // The "about:", "chrome:", and "chrome-native:" schemes are internal to the browser;
        // don't want these to be dispatched to other apps.
        if (params.getUrl().startsWith(ContentUrlConstants.ABOUT_URL_SHORT_PREFIX)
                || params.getUrl().startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX)
                || params.getUrl().startsWith(UrlConstants.CHROME_NATIVE_URL_SHORT_PREFIX)) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Navigating to a chrome-internal page");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // The "content:" scheme is disabled in Clank. Do not try to start an activity.
        if (params.getUrl().startsWith(UrlConstants.CONTENT_URL_SHORT_PREFIX)) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Navigation to content: URL");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // Special case - It makes no sense to use an external application for a YouTube
        // pairing code URL, since these match the current tab with a device (Chromecast
        // or similar) it is supposed to be controlling. Using a different application
        // that isn't expecting this (in particular YouTube) doesn't work.
        if (params.getUrl().matches(".*youtube\\.com(\\/.*)?\\?(.+&)?pairingCode=[^&].+")) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: YouTube URL with a pairing code");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // TODO(changwan): check if we need to handle URL even when external intent is off.
        if (CommandLine.getInstance().hasSwitch(
                ChromeSwitches.DISABLE_EXTERNAL_INTENT_REQUESTS)) {
            Log.w(TAG, "External intent handling is disabled by a command-line flag.");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // http://crbug.com/647569 : Stay in a PWA window for a URL within the same scope.
        @WebappScopePolicy.NavigationDirective
        int webappScopePolicyDirective = mDelegate.applyWebappScopePolicyForUrl(params.getUrl());
        if (webappScopePolicyDirective
                == WebappScopePolicy.NavigationDirective.IGNORE_EXTERNAL_INTENT_REQUESTS) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Stay in PWA window");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // Sanitize the Intent, ensuring web pages can not bypass browser
        // security (only access to BROWSABLE activities).
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setComponent(null);
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }

        List<ResolveInfo> resolvingInfos = mDelegate.queryIntentActivities(intent);
        if (resolvingInfos == null) return OverrideUrlLoadingResult.NO_OVERRIDE;

        boolean canResolveActivity = resolvingInfos.size() > 0;
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        // Check whether the intent can be resolved. If not, we will see whether we can download it
        // from the Market.
        if (!canResolveActivity) {
            if (hasBrowserFallbackUrl) {
                return clobberCurrentTabWithFallbackUrl(browserFallbackUrl, params);
            }

            if (intent.getPackage() != null) {
                String marketReferrer = IntentUtils.safeGetStringExtra(
                        intent, EXTRA_MARKET_REFERRER);
                if (TextUtils.isEmpty(marketReferrer)) {
                    marketReferrer = packageName;
                }
                return sendIntentToMarket(intent.getPackage(), marketReferrer, params);
            }
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Could not find an external activity to use");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (hasBrowserFallbackUrl) {
            intent.removeExtra(EXTRA_BROWSER_FALLBACK_URL);
        }

        final Uri uri = intent.getData();
        if (intent.getPackage() == null && uri != null
                && UrlConstants.SMS_SCHEME.equals(uri.getScheme())) {
            intent.setPackage(getDefaultSmsPackageName(resolvingInfos));
        } else if (uri != null && UrlConstants.TEL_SCHEME.equals(uri.getScheme())
                || (Intent.ACTION_DIAL.equals(intent.getAction()))
                || (Intent.ACTION_CALL.equals(intent.getAction()))) {
            RecordUserAction.record("Android.PhoneIntent");
        }

        // Set the Browser application ID to us in case the user chooses Chrome
        // as the app.  This will make sure the link is opened in the same tab
        // instead of making a new one.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
        if (params.isOpenInNewTab()) intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Ensure intents re-target potential caller activity when we run in CCT mode.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        mDelegate.maybeSetWindowId(intent);
        mDelegate.maybeRecordAppHandlersInIntent(intent, resolvingInfos);

        if (params.getReferrerUrl() != null) {
            IntentHandler.setPendingReferrer(intent, params.getReferrerUrl());
        }

        if (params.isIncognito()) {
            // In incognito mode, links that can be handled within the browser should just do so,
            // without asking the user.
            if (!isExternalProtocol) {
                if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Stay incognito");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }

            IntentHandler.setPendingIncognitoUrl(intent);
        }

        // Make sure webkit can handle it internally before checking for specialized
        // handlers. If webkit can't handle it internally, we need to call
        // startActivityIfNeeded or startActivity.
        if (!isExternalProtocol) {
            if (mDelegate.countSpecializedHandlers(resolvingInfos, intent) == 0) {
                if (incomingIntentRedirect
                        && mDelegate.maybeLaunchInstantApp(
                                   params.getUrl(), params.getReferrerUrl(), true)) {
                    if (DEBUG) Log.i(TAG, "OVERRIDE_WITH_EXTERNAL_INTENT: Instant Apps redirect");
                    return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
                } else if (linkNotFromIntent && !params.isIncognito()
                        && mDelegate.maybeLaunchInstantApp(
                                   params.getUrl(), params.getReferrerUrl(), false)) {
                    if (DEBUG) Log.i(TAG, "OVERRIDE_WITH_EXTERNAL_INTENT: Instant Apps link");
                    return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
                }

                if (DEBUG) Log.i(TAG, "NO_OVERRIDE: No specialized handler for URL");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }

            String delegatePreviousUrl = mDelegate.getPreviousUrl();
            String previousUriString =
                    delegatePreviousUrl != null ? delegatePreviousUrl : params.getReferrerUrl();
            if (previousUriString != null && (isLink || isFormSubmit)) {
                // Current URL has at least one specialized handler available. For navigations
                // within the same host, keep the navigation inside the browser unless the set of
                // available apps to handle the new navigation is different. http://crbug.com/463138
                URI currentUri;
                URI previousUri;

                try {
                    currentUri = new URI(params.getUrl());
                    previousUri = new URI(previousUriString);
                } catch (Exception e) {
                    currentUri = null;
                    previousUri = null;
                }

                if (currentUri != null && previousUri != null
                        && TextUtils.equals(currentUri.getHost(), previousUri.getHost())) {
                    Intent previousIntent;
                    try {
                        previousIntent =
                                Intent.parseUri(previousUriString, Intent.URI_INTENT_SCHEME);
                    } catch (Exception e) {
                        previousIntent = null;
                    }

                    if (previousIntent != null
                            && resolversSubsetOf(resolvingInfos,
                                       mDelegate.queryIntentActivities(previousIntent))) {
                        if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Same host, no new resolvers");
                        return OverrideUrlLoadingResult.NO_OVERRIDE;
                    }
                }
            }
        }

        boolean isDirectInstantAppsIntent =
                isExternalProtocol && InstantAppsHandler.isIntentToInstantApp(intent);
        boolean shouldProxyForInstantApps = isDirectInstantAppsIntent && mDelegate.isSerpReferrer();
        if (shouldProxyForInstantApps) {
            RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                    AiaIntent.SERP, AiaIntent.NUM_ENTRIES);
            intent.putExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER, true);
        } else if (isDirectInstantAppsIntent) {
            // For security reasons, we disable all intent:// URLs to Instant Apps that are
            // not coming from SERP.
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Intent URL to an Instant App");
            RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                    AiaIntent.OTHER, AiaIntent.NUM_ENTRIES);
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        } else {
            // Make sure this extra is not sent unless we've done the verification.
            intent.removeExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER);
        }

        boolean deviceCanHandleIntent = deviceCanHandleIntent(intent);
        if (params.isIncognito() && !mDelegate.willChromeHandleIntent(intent)) {
            // Assume the browser can handle it if there's no activity for this intent.
            if (!deviceCanHandleIntent) {
                if (DEBUG) {
                    Log.i(TAG, "NO_OVERRIDE: Not showing alert dialog with no handler for intent");
                }
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }

            // This intent may leave Chrome.  Warn the user that incognito does not carry over
            // to apps out side of Chrome.
            try {
                if (!mDelegate.startIncognitoIntent(intent, params.getReferrerUrl(),
                            hasBrowserFallbackUrl ? browserFallbackUrl : null, params.getTab(),
                            params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(),
                            shouldProxyForInstantApps)) {
                    if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Failed to show incognito alert dialog.");
                    return OverrideUrlLoadingResult.NO_OVERRIDE;
                }
            } catch (ActivityNotFoundException e) {
                // The activity that we thought was going to handle the intent no longer exists,
                // so catch the exception and assume Chrome can handle it.
                Log.i(TAG, "NO_OVERRIDE: Not showing alert dialog with no handler for intent");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }
            if (DEBUG) Log.i(TAG, "OVERRIDE_WITH_ASYNC_ACTION: Incognito navigation out");
            return OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION;
        }

        // Some third-party app launched Chrome with an intent, and the URL got redirected. The
        // user has explicitly chosen Chrome over other intent handlers, so stay in Chrome
        // unless there was a new intent handler after redirection or Chrome cannot handle it
        // any more.
        // Custom tabs are an exception to this rule, since at no point, the user sees an intent
        // picker and "picking Chrome" is handled inside the support library.
        if (params.getRedirectHandler() != null && incomingIntentRedirect) {
            if (!isExternalProtocol && !params.getRedirectHandler().isFromCustomTabIntent()
                    && !params.getRedirectHandler().hasNewResolver(intent)) {
                if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Custom tab redirect no handled");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }
        }

        // The intent can be used to launch Chrome itself, record the user
        // gesture here so that it can be used later.
        if (params.hasUserGesture()) {
            IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent);
        }

        // If the only specialized intent handler is a WebAPK, set the intent's package to
        // launch the WebAPK without showing the intent picker.
        String targetWebApkPackageName = mDelegate.findWebApkPackageName(resolvingInfos);

        // We can't rely on this falling through to startActivityIfNeeded and behaving
        // correctly for WebAPKs. This is because the target of the intent is the WebApk's main
        // activity but that's just a bouncer which will redirect to WebApkActivity in chrome.
        // To avoid bouncing indefinitely, don't override the navigation if we are currently
        // showing the WebApk |params.webApkPackageName()| that we will redirect to.
        if (targetWebApkPackageName != null
                && targetWebApkPackageName.equals(params.nativeClientPackageName())) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Navigation in WebApk");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (targetWebApkPackageName != null
                && mDelegate.countSpecializedHandlers(resolvingInfos, null) == 1) {
            intent.setPackage(targetWebApkPackageName);
        }

        // http://crbug.com/831806 : Stay in the CCT if the CCT is opened by WebAPK and the url
        // is within the WebAPK scope.
        if (shouldStayInWebappCCT(params, resolvingInfos)) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Navigation in CCT within scope of parent webapp.");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        try {
            if (deviceCanHandleIntent
                    && mDelegate.startActivityIfNeeded(intent, shouldProxyForInstantApps)) {
                // Assume the browser can handle it if there's no activity for this intent.
                if (DEBUG) Log.i(TAG, "OVERRIDE_WITH_EXTERNAL_INTENT: startActivityIfNeeded");
                return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
            }
        } catch (ActivityNotFoundException e) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Activity not found.");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        return OverrideUrlLoadingResult.NO_OVERRIDE;
    }

    /**
     * @return OVERRIDE_WITH_EXTERNAL_INTENT when we successfully started market activity,
     *         NO_OVERRIDE otherwise.
     */
    private @OverrideUrlLoadingResult int sendIntentToMarket(
            String packageName, String marketReferrer, ExternalNavigationParams params) {
        Uri marketUri =
                new Uri.Builder()
                        .scheme("market")
                        .authority("details")
                        .appendQueryParameter(PLAY_PACKAGE_PARAM, packageName)
                        .appendQueryParameter(PLAY_REFERRER_PARAM, Uri.decode(marketReferrer))
                        .build();
        Intent intent = new Intent(Intent.ACTION_VIEW, marketUri);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setPackage("com.android.vending");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (params.getReferrerUrl() != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(params.getReferrerUrl()));
        }

        if (!deviceCanHandleIntent(intent)) {
            // Exit early if the Play Store isn't available. (https://crbug.com/820709)
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Play Store not installed.");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        if (params.isIncognito()) {
            if (!mDelegate.startIncognitoIntent(intent, params.getReferrerUrl(), null,
                        params.getTab(),
                        params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(), false)) {
                if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Failed to show incognito alert dialog.");
                return OverrideUrlLoadingResult.NO_OVERRIDE;
            }
            if (DEBUG) Log.i(TAG, "OVERRIDE_WITH_ASYNC_ACTION: Incognito intent to Play Store.");
            return OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION;
        } else {
            mDelegate.startActivity(intent, false);
            if (DEBUG) Log.i(TAG, "OVERRIDE_WITH_EXTERNAL_INTENT: Intent to Play Store.");
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }
    }

    /**
     * Clobber the current tab with fallback URL.
     *
     * @param browserFallbackUrl The fallback URL.
     * @param params The external navigation params.
     * @return {@link OverrideUrlLoadingResult} if the tab was clobbered, or we launched an
     *         intent.
     */
    private @OverrideUrlLoadingResult int clobberCurrentTabWithFallbackUrl(
            String browserFallbackUrl, ExternalNavigationParams params) {
        // If the fallback URL is a link to Play Store, send the user to Play Store app
        // instead: crbug.com/638672.
        Pair<String, String> appInfo = maybeGetPlayStoreAppIdAndReferrer(browserFallbackUrl);
        if (appInfo != null) {
            String marketReferrer = TextUtils.isEmpty(appInfo.second)
                    ? ContextUtils.getApplicationContext().getPackageName() : appInfo.second;
            return sendIntentToMarket(appInfo.first, marketReferrer, params);
        }

        // For subframes, we don't support fallback url for now.
        // http://crbug.com/364522.
        if (!params.isMainFrame()) {
            if (DEBUG) Log.i(TAG, "NO_OVERRIDE: Don't support fallback url in subframes");
            return OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        // NOTE: any further redirection from fall-back URL should not override URL loading.
        // Otherwise, it can be used in chain for fingerprinting multiple app installation
        // status in one shot. In order to prevent this scenario, we notify redirection
        // handler that redirection from the current navigation should stay in Chrome.
        if (params.getRedirectHandler() != null) {
            params.getRedirectHandler().setShouldNotOverrideUrlLoadingUntilNewUrlLoading();
        }
        if (DEBUG) Log.i(TAG, "OVERRIDE: clobberCurrentTab called");
        return mDelegate.clobberCurrentTab(browserFallbackUrl, params.getReferrerUrl());
    }

    /**
     * If the given URL is to Google Play, extracts the package name and referrer tracking code
     * from the {@param url} and returns as a Pair in that order. Otherwise returns null.
     */
    private Pair<String, String> maybeGetPlayStoreAppIdAndReferrer(String url) {
        Uri uri = Uri.parse(url);
        if (PLAY_HOSTNAME.equals(uri.getHost()) && uri.getPath() != null
                && uri.getPath().startsWith(PLAY_APP_PATH)
                && !TextUtils.isEmpty(uri.getQueryParameter(PLAY_PACKAGE_PARAM))) {
            return new Pair<String, String>(uri.getQueryParameter(PLAY_PACKAGE_PARAM),
                    uri.getQueryParameter(PLAY_REFERRER_PARAM));
        }
        return null;
    }

    /**
     * @return Whether the |url| could be handled by an external application on the system.
     */
    public boolean canExternalAppHandleUrl(String url) {
        if (url.startsWith(WTAI_MC_URL_PREFIX)) return true;
        try {
            Intent intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
            if (intent.getPackage() != null) return true;

            List<ResolveInfo> resolvingInfos = mDelegate.queryIntentActivities(intent);
            if (resolvingInfos != null && resolvingInfos.size() > 0) return true;
        } catch (Exception ex) {
            // Ignore the error.
            Log.w(TAG, "Bad URI %s", url, ex);
        }
        return false;
    }

    /**
     * Dispatch SMS intents to the default SMS application if applicable.
     * Most SMS apps refuse to send SMS if not set as default SMS application.
     *
     * @param resolvingComponentNames The list of ComponentName that resolves the current intent.
     */
    private String getDefaultSmsPackageName(List<ResolveInfo> resolvingComponentNames) {
        String defaultSmsPackageName = mDelegate.getDefaultSmsPackageName();
        if (defaultSmsPackageName == null) return null;
        // Makes sure that the default SMS app actually resolves the intent.
        for (ResolveInfo resolveInfo : resolvingComponentNames) {
            if (defaultSmsPackageName.equals(resolveInfo.activityInfo.packageName)) {
                return defaultSmsPackageName;
            }
        }
        return null;
    }

    // Returns whether a navigation in a CustomTabActivity opened from a WebAPK/TWA should stay
    // within the CustomTabActivity. Returns false if the navigation does not occur within a
    // CustomTabActivity or the CustomTabActivity was not opened from a WebAPK/TWA.
    private boolean shouldStayInWebappCCT(
            ExternalNavigationParams params, List<ResolveInfo> handlers) {
        Tab tab = params.getTab();
        if (tab == null || !tab.isCurrentlyACustomTab() || tab.getActivity() == null) {
            return false;
        }

        int launchSource = IntentUtils.safeGetIntExtra(
                tab.getActivity().getIntent(), EXTRA_BROWSER_LAUNCH_SOURCE, LaunchSourceType.OTHER);
        if (launchSource != LaunchSourceType.WEBAPK) {
            return false;
        }

        String appId = IntentUtils.safeGetStringExtra(
                tab.getActivity().getIntent(), Browser.EXTRA_APPLICATION_ID);
        if (appId == null) return false;

        try {
            Intent.parseUri(params.getUrl(), Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ex) {
            return false;
        }
        return ExternalNavigationDelegateImpl
                       .getSpecializedHandlersWithFilter(handlers, appId, null)
                       .size()
                > 0;
    }

    /**
     * Returns whether or not there's an activity available to handle the intent.
     */
    private boolean deviceCanHandleIntent(Intent intent) {
        List<ResolveInfo> resolveInfos = mDelegate.queryIntentActivities(intent);
        return resolveInfos != null && resolveInfos.size() > 0;
    }
}
