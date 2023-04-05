// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.directactions.DirectActionCoordinator;
import org.chromium.chrome.browser.feedback.FeedbackReporter;
import org.chromium.chrome.browser.gsa.GSAHelper;
import org.chromium.chrome.browser.historyreport.AppIndexingReporter;
import org.chromium.chrome.browser.init.ChromeStartupDelegate;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.metrics.VariationsSession;
import org.chromium.chrome.browser.notifications.chime.ChimeDelegate;
import org.chromium.chrome.browser.omaha.RequestGenerator;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmark;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksProviderIterator;
import org.chromium.chrome.browser.password_manager.GooglePasswordManagerUIProvider;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.survey.SurveyController;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.ui.signin.GoogleActivityController;
import org.chromium.chrome.browser.usage_stats.DigitalWellbeingClient;
import org.chromium.chrome.browser.webapps.GooglePlayWebApkInstallDelegate;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.policy.AppRestrictionsProvider;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.SystemAccountManagerDelegate;
import org.chromium.components.webapps.AppDetailsDelegate;

import java.util.Collections;
import java.util.List;

/**
 * Base class for defining methods where different behavior is required by downstream targets.
 * The correct version of {@link AppHooksImpl} will be determined at compile time via build rules.
 * See http://crbug/560466.
 *
 * Note that new functionality should not be added to AppHooks. Instead the delegate pattern in
 * go/apphooks-migration should be followed to solve this class of problems.
 */
public abstract class AppHooks {
    private static AppHooksImpl sInstanceForTesting;

    /**
     * Sets a mocked instance for testing.
     */
    @VisibleForTesting
    public static void setInstanceForTesting(AppHooksImpl instance) {
        sInstanceForTesting = instance;
    }

    public static AppHooks get() {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        // R8 can better optimize if we return a new instance each time.
        return new AppHooksImpl();
    }

    /**
     * Creates a new {@link AccountManagerDelegate}.
     * @return the created {@link AccountManagerDelegate}.
     */
    public AccountManagerDelegate createAccountManagerDelegate() {
        return new SystemAccountManagerDelegate();
    }

    /**
     * @return An instance of AppDetailsDelegate that can be queried about app information for the
     *         App Banner feature.  Will be null if one is unavailable.
     */
    public AppDetailsDelegate createAppDetailsDelegate() {
        return null;
    }

    /**
     * Creates a new {@link AppIndexingReporter}.
     * @return the created {@link AppIndexingReporter}.
     */
    public AppIndexingReporter createAppIndexingReporter() {
        return new AppIndexingReporter();
    }

    /**
     * @return An instance of {@link CustomTabsConnection}. Should not be called
     * outside of {@link CustomTabsConnection#getInstance()}.
     */
    public CustomTabsConnection createCustomTabsConnection() {
        return new CustomTabsConnection();
    }

    /**
     * Returns a new {@link DirectActionCoordinator} instance, if available.
     */
    @Nullable
    public DirectActionCoordinator createDirectActionCoordinator() {
        return null;
    }

    /**
     * Creates a new {@link SurveyController}.
     * @return The created {@link SurveyController}.
     */
    public SurveyController createSurveyController() {
        return new SurveyController();
    }

    /**
     * @return An instance of {@link FeedbackReporter} to report feedback.
     */
    public FeedbackReporter createFeedbackReporter() {
        return new FeedbackReporter() {};
    }

    /**
     * @return An instance of GoogleActivityController.
     */
    public GoogleActivityController createGoogleActivityController() {
        return new GoogleActivityController();
    }

    /**
     * @return An instance of {@link GSAHelper} that handles the start point of chrome's integration
     *         with GSA.
     */
    public GSAHelper createGsaHelper() {
        return new GSAHelper();
    }

    public InstantAppsHandler createInstantAppsHandler() {
        return new InstantAppsHandler();
    }

    /**
     * @return An instance of {@link GooglePasswordManagerUIProvider}. Will be null if one is not
     *         available.
     */
    public GooglePasswordManagerUIProvider createGooglePasswordManagerUIProvider() {
        return null;
    }

    /**
     * @return An instance of RequestGenerator to be used for Omaha XML creation.  Will be null if
     *         a generator is unavailable.
     */
    public RequestGenerator createOmahaRequestGenerator() {
        return null;
    }

    /**
     * @return a new {@link ProcessInitializationHandler} instance.
     */
    public ProcessInitializationHandler createProcessInitializationHandler() {
        return new ProcessInitializationHandler();
    }

    /**
     * @return An instance of RevenueStats to be installed as a singleton.
     */
    public RevenueStats createRevenueStatsInstance() {
        return new RevenueStats();
    }

    /**
     * Returns a new instance of VariationsSession.
     */
    public VariationsSession createVariationsSession() {
        return new VariationsSession();
    }

    /** Returns the singleton instance of GooglePlayWebApkInstallDelegate. */
    public GooglePlayWebApkInstallDelegate getGooglePlayWebApkInstallDelegate() {
        return null;
    }

    /**
     * @return An instance of PolicyAuditor that notifies the policy system of the user's activity.
     * Only applicable when the user has a policy active, that is tracking the activity.
     */
    public PolicyAuditor getPolicyAuditor() {
        return null;
    }

    public void registerPolicyProviders(CombinedPolicyProvider combinedProvider) {
        combinedProvider.registerProvider(
                new AppRestrictionsProvider(ContextUtils.getApplicationContext()));
    }

    /**
     * @return A list of allowlisted app package names whose completed notifications
     * we should suppress.
     */
    public List<String> getOfflinePagesSuppressNotificationPackages() {
        return Collections.emptyList();
    }

    /**
     * @return An iterator of partner bookmarks.
     */
    @Nullable
    public PartnerBookmark.BookmarkIterator getPartnerBookmarkIterator() {
        return PartnerBookmarksProviderIterator.createIfAvailable();
    }

    /**
     * @return A new {@link DigitalWellbeingClient} instance.
     */
    public DigitalWellbeingClient createDigitalWellbeingClient() {
        return new DigitalWellbeingClient();
    }

    /**
     * Checks the Google Play services availability on the this device.
     *
     * This is a workaround for the
     * versioned API of {@link GoogleApiAvailability#isGooglePlayServicesAvailable()}. The current
     * Google Play services SDK version doesn't have this API yet.
     *
     * TODO(zqzhang): Remove this method after the SDK is updated.
     *
     * @return status code indicating whether there was an error. The possible return values are the
     * same as {@link GoogleApiAvailability#isGooglePlayServicesAvailable()}.
     */
    public int isGoogleApiAvailableWithMinApkVersion(int minApkVersion) {
        int apkVersion =
                PackageUtils.getPackageVersion(GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE);
        return apkVersion < 0                ? ConnectionResult.SERVICE_MISSING
                : apkVersion < minApkVersion ? ConnectionResult.SERVICE_VERSION_UPDATE_REQUIRED
                                             : ConnectionResult.SUCCESS;
    }

    /**
     * Returns a new {@link TrustedVaultClient.Backend} instance.
     */
    public TrustedVaultClient.Backend createSyncTrustedVaultClientBackend() {
        return new TrustedVaultClient.EmptyBackend();
    }

    /**
     * This is deprecated, and should not be called. Use FeedHooks instead.
     */
    public @Nullable ProcessScope getExternalSurfaceProcessScope(
            ProcessScopeDependencyProvider dependencies) {
        return null;
    }

    /**
     * Returns the URL to the WebAPK creation/update server.
     */
    public String getWebApkServerUrl() {
        return "";
    }

    /**
     * Returns a Chime Delegate if the chime module is defined.
     */
    public ChimeDelegate getChimeDelegate() {
        return new ChimeDelegate();
    }

    public @Nullable ImageEditorModuleProvider getImageEditorModuleProvider() {
        return null;
    }

    public ChromeStartupDelegate createChromeStartupDelegate() {
        return new ChromeStartupDelegate();
    }

    public boolean canStartForegroundServiceWhileInvisible() {
        return true;
    }

    public String getDefaultQueryTilesServerUrl() {
        return "";
    }

    // Stop! Do not add new methods to AppHooks anymore. Follow go/apphooks-migration instead.
}
