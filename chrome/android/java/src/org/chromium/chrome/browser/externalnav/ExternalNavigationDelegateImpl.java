// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Function;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
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
    public boolean shouldDisableExternalIntentRequestsForUrl(GURL url) {
        return false;
    }

    @Override
    public boolean handlesInstantAppLaunchingInternally() {
        return true;
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
    public boolean hasCustomLeavingIncognitoDialog() {
        return false;
    }

    @Override
    public void presentLeavingIncognitoModalDialog(Callback<Boolean> onUserDecision) {
        // This should never be called due to returning false in
        // hasCustomLeavingIncognitoDialog().
        assert false;
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
    public void maybeSetPendingReferrer(Intent intent, GURL referrerUrl) {
        IntentHandler.setPendingReferrer(intent, referrerUrl);
    }

    @Override
    public void maybeSetPendingIncognitoUrl(Intent intent) {
        IntentHandler.setPendingIncognitoUrl(intent);
    }

    @Override
    public boolean maybeLaunchInstantApp(GURL url, GURL referrerUrl, boolean isIncomingRedirect,
            boolean isSerpReferrer, Supplier<List<ResolveInfo>> resolveInfoSupplier) {
        if (!hasValidTab() || mTab.getWebContents() == null) return false;

        InstantAppsHandler handler = InstantAppsHandler.getInstance();
        RedirectHandler redirect = RedirectHandlerTabHelper.getHandlerFor(mTab);
        Intent intent = redirect != null ? redirect.getInitialIntent() : null;
        // TODO(mariakhomenko): consider also handling NDEF_DISCOVER action redirects.
        if (isIncomingRedirect && intent != null && Intent.ACTION_VIEW.equals(intent.getAction())) {
            // Set the URL the redirect was resolved to for checking the existence of the
            // instant app inside handleIncomingIntent().
            Intent resolvedIntent = new Intent(intent);
            resolvedIntent.setData(Uri.parse(url.getSpec()));
            return handler.handleIncomingIntent(getAvailableContext(), resolvedIntent,
                    LaunchIntentDispatcher.isCustomTabIntent(resolvedIntent), true,
                    resolveInfoSupplier);
        } else if (!isIncomingRedirect) {
            // Check if the navigation is coming from SERP and skip instant app handling.
            if (isSerpReferrer) return false;
            return handler.handleNavigation(getAvailableContext(), url, referrerUrl, mTab);
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
    protected void startAutofillAssistantWithIntent(Intent targetIntent, GURL browserFallbackUrl) {
        AutofillAssistantFacade.start(
                TabUtils.getActivity(mTab), targetIntent.getExtras(), browserFallbackUrl.getSpec());
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
    public boolean isIntentForTrustedCallingApp(
            Intent intent, Supplier<List<ResolveInfo>> resolveInfoSupplier) {
        return false;
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
            GURL browserFallbackUrl, boolean isGoogleReferrer) {
        if (!browserFallbackUrl.isEmpty() && !params.isIncognito()
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

    @Override
    public boolean shouldLaunchWebApksOnInitialIntent() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && ChromeFeatureList.sWebApkTrampolineOnInitialIntent.isEnabled();
    }

    @Override
    public boolean maybeSetTargetPackage(
            Intent intent, Supplier<List<ResolveInfo>> resolveInfoSupplier) {
        return false;
    }

    @Override
    public boolean shouldAvoidDisambiguationDialog(Intent intent) {
        return false;
    }

    @Override
    public boolean shouldEmbedderInitiatedNavigationsStayInBrowser() {
        // The initial navigation off of things like typed navigations or bookmarks should stay in
        // the browser.
        return true;
    }
}
