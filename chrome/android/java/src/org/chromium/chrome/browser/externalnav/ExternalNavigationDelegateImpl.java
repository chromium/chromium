// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;

import org.chromium.base.ApkInfo;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.password_manager.CctPasswordSavingMetricsRecorderBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.List;
import java.util.function.Predicate;
import java.util.function.Supplier;

/** The main implementation of the {@link ExternalNavigationDelegate}. */
@NullMarked
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    protected final Context mApplicationContext;
    private final Tab mTab;
    private final TabObserver mTabObserver;
    private final @Nullable Supplier<TabModelSelector> mTabModelSelectorSupplier;

    private boolean mIsTabDestroyed;
    private @TabLaunchType int mTabLaunchType;

    private static @Nullable Predicate<Intent> sWillChromeHandleIntentHookForTesting;

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
        mTabLaunchType = tab.getLaunchType();
    }

    @Override
    public @Nullable Context getContext() {
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

    public static void setWillChromeHandleIntentHookForTesting(Predicate<Intent> hook) {
        sWillChromeHandleIntentHookForTesting = hook;
        ResettersForTesting.register(() -> sWillChromeHandleIntentHookForTesting = null);
    }

    /**
     * Determines whether Chrome would handle this Intent if fired immediately. Note that this does
     * not guarantee that Chrome actually will handle the intent, as another app may be installed,
     * or components may be enabled that provide alternative handlers for this intent before it gets
     * fired.
     *
     * @param intent Intent that will be fired.
     * @param matchDefaultOnly See {@link PackageManager#MATCH_DEFAULT_ONLY}.
     * @return True if Chrome will definitely handle the intent, false otherwise.
     */
    public static boolean willChromeHandleIntent(Intent intent, boolean matchDefaultOnly) {
        if (sWillChromeHandleIntentHookForTesting != null) {
            return sWillChromeHandleIntentHookForTesting.test(intent);
        }
        // Early-out if the intent targets Chrome.
        if (IntentUtils.intentTargetsSelf(intent)) return true;

        // Fall back to the more expensive querying of Android when the intent doesn't target
        // Chrome.
        ResolveInfo info =
                PackageManagerUtils.resolveActivity(
                        intent, matchDefaultOnly ? PackageManager.MATCH_DEFAULT_ONLY : 0);
        if (info == null) return false;
        return info.activityInfo.packageName.equals(ApkInfo.getHostPackageName());
    }

    @Override
    public boolean willAppHandleIntent(Intent intent) {
        return willChromeHandleIntent(intent, false);
    }

    @Override
    public boolean shouldDisableExternalIntentRequestsForUrl(
            ExternalNavigationParams params, Intent intent) {
        return false;
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
        if (mTabModelSelectorSupplier == null) return;
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        if (tabModelSelector == null) return;
        tabModelSelector.tryCloseTab(
                TabClosureParams.closeTab(mTab).allowUndo(false).build(), /* allowDialog= */ false);
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
    public @Nullable WindowAndroid getWindowAndroid() {
        if (mTab == null) return null;
        return mTab.getWindowAndroid();
    }

    @Override
    public @Nullable WebContents getWebContents() {
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
    public boolean canCloseTabOnIntentLaunch() {
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

    @Override
    public void notifyCctPasswordSavingRecorderOfExternalNavigation() {
        WindowAndroid windowAndroid = assumeNonNull(getWindowAndroid());
        CctPasswordSavingMetricsRecorderBridge cctSavingMetricsRecorder =
                CctPasswordSavingMetricsRecorderBridge.KEY.retrieveDataFromHost(
                        windowAndroid.getUnownedUserDataHost());
        if (cctSavingMetricsRecorder != null) {
            cctSavingMetricsRecorder.onExternalNavigation();
        }
    }

    @Override
    public void reportIntentToSafeBrowsing(Intent intent) {
        SafeBrowsingBridge.reportIntent(assertNonNull(mTab.getWebContents()), intent);
    }

    @Override
    public @Nullable Intent createIntentToPreventIncognitoAccess(GURL url) {
        if (!url.getSpec().startsWith(UrlConstants.CHROME_EXTENSIONS_URL)) {
            return null;
        }
        Intent intent = new Intent(getContext(), ChromeLauncherActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url.getSpec()));
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);

        return intent;
    }

    @Override
    public boolean wasTabLaunchedFromLinkCreatingNewForegroundTab() {
        return mTabLaunchType == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || mTabLaunchType == TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP;
    }

    @Override
    public boolean wasTabLaunchedFromLinkCreatingNewWindow() {
        return mTabLaunchType == TabLaunchType.FROM_LINK_CREATING_NEW_WINDOW;
    }

    /**
     * Sets the {@link TabLaunchType} for this delegate for testing purposes. This has no effect on
     * the related Tab launch type.
     *
     * @param launchType The {@link TabLaunchType} to set for this delegate.
     */
    public void setTabLaunchTypeForTesting(@TabLaunchType int launchType) {
        @TabLaunchType int originalTabLaunchType = mTabLaunchType;
        mTabLaunchType = launchType;
        ResettersForTesting.register(() -> mTabLaunchType = originalTabLaunchType);
    }
}
