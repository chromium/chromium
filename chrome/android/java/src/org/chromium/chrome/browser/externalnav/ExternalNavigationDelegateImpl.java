// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Function;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.instantapps.AuthenticatedProxyActivity;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.components.external_intents.ExternalNavigationDelegate.StartActivityIfNeededResult;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.util.List;

/**
 * The main implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    protected final Context mApplicationContext;
    private final Tab mTab;
    private final TabObserver mTabObserver;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;

    private boolean mIsTabDestroyed;

    public ExternalNavigationDelegateImpl(Tab tab) {
        mTab = tab;
        mTabModelSelectorSupplier = TabModelSelectorSupplier.from(tab.getWindowAndroid());
        mApplicationContext = ContextUtils.getApplicationContext();
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onDestroyed(Tab tab) {
                mIsTabDestroyed = true;
            }
        };
        mTab.addObserver(mTabObserver);
    }

    @Override
    public Context getContext() {
        if (mTab.getWindowAndroid() == null) return null;
        return mTab.getWindowAndroid().getContext().get();
    }

    /**
     * Gets the {@link Activity} linked to this instance if it is available. At times this object
     * might not have an associated Activity, in which case the ApplicationContext is returned.
     * @return The activity {@link Context} if it can be reached.
     *         Application {@link Context} if not.
     */
    protected final Context getAvailableContext() {
        Activity activityContext = ContextUtils.activityFromContext(getContext());
        if (activityContext == null) return ContextUtils.getApplicationContext();
        return activityContext;
    }

    /**
     * Determines whether Chrome would handle this Intent if fired immediately. Note that this does
     * not guarantee that Chrome actually will handle the intent, as another app may be installed,
     * or components may be enabled that provide alternative handlers for this intent before it gets
     * fired.
     *
     * @param intent            Intent that will be fired.
     * @param matchDefaultOnly  See {@link PackageManager#MATCH_DEFAULT_ONLY}.
     * @return                  True if Chrome will definitely handle the intent, false otherwise.
     */
    public static boolean willChromeHandleIntent(Intent intent, boolean matchDefaultOnly) {
        Context context = ContextUtils.getApplicationContext();
        // Early-out if the intent targets Chrome.
        if (IntentUtils.intentTargetsSelf(context, intent)) return true;

        // Fall back to the more expensive querying of Android when the intent doesn't target
        // Chrome.
        ResolveInfo info = PackageManagerUtils.resolveActivity(
                intent, matchDefaultOnly ? PackageManager.MATCH_DEFAULT_ONLY : 0);
        return info != null && info.activityInfo.packageName.equals(context.getPackageName());
    }

    @Override
    public boolean willAppHandleIntent(Intent intent) {
        return willChromeHandleIntent(intent, false);
    }

    @Override
    public boolean shouldDisableExternalIntentRequestsForUrl(String url) {
        return false;
    }

    @Override
    public boolean handlesInstantAppLaunchingInternally() {
        return true;
    }

    /**
     * Check whether the given package is a specialized handler for the given intent
     *
     * @param packageName Package name to check against. Can be null or empty.
     * @param intent The intent to resolve for.
     * @return Whether the given package is a specialized handler for the given intent. If there is
     *         no package name given checks whether there is any specialized handler.
     */
    public static boolean isPackageSpecializedHandler(String packageName, Intent intent) {
        List<ResolveInfo> handlers = PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.GET_RESOLVED_FILTER);
        return !ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(handlers, packageName, true)
                        .isEmpty();
    }

    @Override
    public void didStartActivity(Intent intent) {}

    @Override
    public @StartActivityIfNeededResult int maybeHandleStartActivityIfNeeded(
            Intent intent, boolean proxy) {
        return StartActivityIfNeededResult.DID_NOT_HANDLE;
    }

    @Override
    public OverrideUrlLoadingResult handleIncognitoIntentTargetingSelf(
            final Intent intent, final String referrerUrl, final String fallbackUrl) {
        String primaryUrl = intent.getDataString();
        boolean isUrlLoadedInTheSameTab = ExternalNavigationHandler.loadUrlFromIntent(
                referrerUrl, primaryUrl, fallbackUrl, this, false, true);
        return (isUrlLoadedInTheSameTab) ? OverrideUrlLoadingResult.forClobberingTab()
                                         : OverrideUrlLoadingResult.forExternalIntent();
    }

    @Override
    public boolean supportsCreatingNewTabs() {
        return true;
    }

    @Override
    public void loadUrlInNewTab(final String url, final boolean launchIncognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
        if (launchIncognito) intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setClassName(packageName, ChromeLauncherActivity.class.getName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentHandler.addTrustedIntentExtras(intent);
        ExternalNavigationHandler.startActivity(intent, false, this);
    }

    @Override
    public boolean canLoadUrlInCurrentTab() {
        return !(mTab == null || mTab.isClosing() || !mTab.isInitialized());
    }

    @Override
    public void loadUrlIfPossible(LoadUrlParams loadUrlParams) {
        if (!hasValidTab()) return;
        mTab.loadUrl(loadUrlParams);
    }

    @Override
    public boolean isApplicationInForeground() {
        return ApplicationStatus.getStateForApplication()
                == ApplicationState.HAS_RUNNING_ACTIVITIES;
    }

    @Override
    public void maybeSetWindowId(Intent intent) {
        Context context = getAvailableContext();
        if (!(context instanceof ChromeTabbedActivity2)) return;
        intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, 2);
    }

    @Override
    public void closeTab() {
        if (!hasValidTab()) return;
        if (!mTabModelSelectorSupplier.hasValue()) return;
        mTabModelSelectorSupplier.get().closeTab(mTab);
    }

    @Override
    public boolean isIncognito() {
        return mTab.isIncognito();
    }

    @Override
    public void maybeAdjustInstantAppExtras(Intent intent, boolean isIntentToInstantApp) {
        if (isIntentToInstantApp) {
            intent.putExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER, true);
        } else {
            // Make sure this extra is not sent unless we've done the verification.
            intent.removeExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER);
        }
    }

    @Override
    public void maybeSetRequestMetadata(Intent intent, boolean hasUserGesture,
            boolean isRendererInitiated, @Nullable Origin initiatorOrigin) {
        if (!hasUserGesture && !isRendererInitiated && initiatorOrigin == null) return;
        // The intent can be used to launch Chrome itself, record the user
        // gesture, whether request is renderer initiated and initiator origin here so that it can
        // be used later.
        IntentWithRequestMetadataHandler.RequestMetadata metadata =
                new IntentWithRequestMetadataHandler.RequestMetadata(
                        hasUserGesture, isRendererInitiated, initiatorOrigin);
        IntentWithRequestMetadataHandler.getInstance().onNewIntentWithRequestMetadata(
                intent, metadata);
    }

    @Override
    public void maybeSetPendingReferrer(Intent intent, String referrerUrl) {
        IntentHandler.setPendingReferrer(intent, referrerUrl);
    }

    @Override
    public void maybeSetPendingIncognitoUrl(Intent intent) {
        IntentHandler.setPendingIncognitoUrl(intent);
    }

    @Override
    public boolean maybeLaunchInstantApp(
            String url, String referrerUrl, boolean isIncomingRedirect, boolean isSerpReferrer) {
        if (!hasValidTab() || mTab.getWebContents() == null) return false;

        InstantAppsHandler handler = InstantAppsHandler.getInstance();
        RedirectHandler redirect = RedirectHandlerTabHelper.getHandlerFor(mTab);
        Intent intent = redirect != null ? redirect.getInitialIntent() : null;
        // TODO(mariakhomenko): consider also handling NDEF_DISCOVER action redirects.
        if (isIncomingRedirect && intent != null && Intent.ACTION_VIEW.equals(intent.getAction())) {
            // Set the URL the redirect was resolved to for checking the existence of the
            // instant app inside handleIncomingIntent().
            Intent resolvedIntent = new Intent(intent);
            resolvedIntent.setData(Uri.parse(url));
            return handler.handleIncomingIntent(getAvailableContext(), resolvedIntent,
                    LaunchIntentDispatcher.isCustomTabIntent(resolvedIntent), true);
        } else if (!isIncomingRedirect) {
            // Check if the navigation is coming from SERP and skip instant app handling.
            if (isSerpReferrer) return false;
            return handler.handleNavigation(getAvailableContext(), url,
                    TextUtils.isEmpty(referrerUrl) ? null : Uri.parse(referrerUrl), mTab);
        }
        return false;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        if (mTab == null) return null;
        return mTab.getWindowAndroid();
    }

    @Override
    public WebContents getWebContents() {
        if (mTab == null) return null;
        return mTab.getWebContents();
    }

    @Override
    public void dispatchAuthenticatedIntent(Intent intent) {
        Intent proxyIntent = new Intent(Intent.ACTION_MAIN);
        proxyIntent.setClass(getAvailableContext(), AuthenticatedProxyActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        proxyIntent.putExtra(AuthenticatedProxyActivity.AUTHENTICATED_INTENT_EXTRA, intent);
        getAvailableContext().startActivity(proxyIntent);
    }

    /**
     * Starts the autofill assistant with the given intent. Exists to allow tests to stub out this
     * functionality.
     */
    protected void startAutofillAssistantWithIntent(
            Intent targetIntent, String browserFallbackUrl) {
        AutofillAssistantFacade.start(
                TabUtils.getActivity(mTab), targetIntent.getExtras(), browserFallbackUrl);
    }

    /**
     * @return Whether or not we have a valid {@link Tab} available.
     */
    @Override
    public boolean hasValidTab() {
        return mTab != null && !mIsTabDestroyed;
    }

    @Override
    public boolean canCloseTabOnIncognitoIntentLaunch() {
        return (mTab != null && !mTab.isClosing() && mTab.isInitialized());
    }

    @Override
    public boolean isIntentForTrustedCallingApp(Intent intent) {
        return false;
    }

    @Override
    public boolean isIntentToInstantApp(Intent intent) {
        return InstantAppsHandler.isIntentToInstantApp(intent);
    }

    @Override
    public boolean isIntentToAutofillAssistant(Intent intent) {
        return AutofillAssistantFacade.isAutofillAssistantByIntentTriggeringEnabled(intent);
    }

    @Override
    public @IntentToAutofillAllowingAppResult int isIntentToAutofillAssistantAllowingApp(
            ExternalNavigationParams params, Intent targetIntent,
            Function<Intent, Boolean> canExternalAppHandleIntent) {
        if (params.isIncognito()) {
            return IntentToAutofillAllowingAppResult.NONE;
        }
        return AutofillAssistantFacade.shouldAllowOverrideWithApp(
                targetIntent, canExternalAppHandleIntent);
    }

    @Override
    public boolean handleWithAutofillAssistant(ExternalNavigationParams params, Intent targetIntent,
            String browserFallbackUrl, boolean isGoogleReferrer) {
        if (browserFallbackUrl != null && !params.isIncognito()
                && AutofillAssistantFacade.isAutofillAssistantByIntentTriggeringEnabled(
                        targetIntent)
                && isGoogleReferrer) {
            if (mTab != null) {
                startAutofillAssistantWithIntent(targetIntent, browserFallbackUrl);
            }
            return true;
        }
        return false;
    }
}
