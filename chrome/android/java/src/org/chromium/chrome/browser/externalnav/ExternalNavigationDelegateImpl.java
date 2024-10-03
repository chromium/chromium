// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.List;

/** The main implementation of the {@link ExternalNavigationDelegate}. */
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
        mTabObserver =
                new EmptyTabObserver() {
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
        ResolveInfo info =
                PackageManagerUtils.resolveActivity(
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
    public boolean canLoadUrlInCurrentTab() {
        return !(mTab == null || mTab.isClosing() || !mTab.isInitialized());
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
    public void maybeSetRequestMetadata(
            Intent intent, boolean hasUserGesture, boolean isRendererInitiated) {
        if (!hasUserGesture && !isRendererInitiated) return;
        // The intent can be used to launch Chrome itself, record the user
        // gesture, whether request is renderer initiated and initiator origin here so that it can
        // be used later.
        IntentWithRequestMetadataHandler.RequestMetadata metadata =
                new IntentWithRequestMetadataHandler.RequestMetadata(
                        hasUserGesture, isRendererInitiated);
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent, metadata);
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
    public WindowAndroid getWindowAndroid() {
        if (mTab == null) return null;
        return mTab.getWindowAndroid();
    }

    @Override
    public WebContents getWebContents() {
        if (mTab == null) return null;
        return mTab.getWebContents();
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
    public boolean isForTrustedCallingApp(Supplier<List<ResolveInfo>> resolveInfoSupplier) {
        return false;
    }

    @Override
    public boolean shouldLaunchWebApksOnInitialIntent() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S;
    }

    @Override
    public void setPackageForTrustedCallingApp(Intent intent) {
        assert false;
    }

    @Override
    public boolean shouldAvoidDisambiguationDialog(GURL intentDataUrl) {
        return false;
    }

    @Override
    public boolean shouldEmbedderInitiatedNavigationsStayInBrowser() {
        // The initial navigation off of things like typed navigations or bookmarks should stay in
        // the browser.
        return true;
    }

    @Override
    public String getSelfScheme() {
        return IntentHandler.GOOGLECHROME_SCHEME;
    }

    @Override
    public boolean shouldDisableAllExternalIntents() {
        return false;
    }

    @Override
    public boolean shouldReturnAsActivityResult(GURL url) {
        return false;
    }

    @Override
    public void returnAsActivityResult(GURL url) {
        throw new UnsupportedOperationException("Returning as activity result is not supported.");
    }

    @Override
    public void maybeRecordExternalNavigationSchemeHistogram(GURL url) {}
}
