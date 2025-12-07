// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.MoreExecutors;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.Account;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AnalyzeAttachment;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.AvailabilityResponse;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.Capability;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.ClientInfo;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.File;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.SummarizeUrl;
import org.chromium.chrome.browser.ai.proto.SystemAiProviderService.UrlContext;
import org.chromium.chrome.browser.content_extraction.InnerTextBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.Duration;

/*
 * Service to interact with an AI assistant, used to invoke an assistant UI to summarize web pages
 * and/or ask questions about it. System-specific integration is handled with the SystemAiProvider
 * interface, and if none is present it falls back to sending an ACTION_VOICE_COMMAND intent to
 * invoke the voice assistant UI.
 */
@NullMarked
public class AiAssistantService {

    private static final String TAG = "AiAssistantService";
    static final String EXTRA_LAUNCH_REQUEST =
            "org.chromium.chrome.browser.ai.proto.SystemAiProviderService.LaunchRequest";

    private boolean mIsSummarizeAvailable;
    private boolean mIsAnalyzeAttachmentAvailable;
    private boolean mIsSystemAiProviderAvailable;
    private boolean mIsInitializing;
    private boolean mIsInitialized;
    private final SystemAiProvider mSystemAiProvider;

    @Nullable private static AiAssistantService sInstance;
    private final SharedPreferencesManager mSharedPreferencesManager;

    @IntDef({
        PrefLoadingResult.LOADED,
        PrefLoadingResult.NOT_AVAILABLE,
        PrefLoadingResult.EXPIRED,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface PrefLoadingResult {
        // Pref contains an unexpired result, it was loaded.
        int LOADED = 0;
        // Pref didn't contain a result.
        int NOT_AVAILABLE = 1;
        // Pref contained an expired result, it was loaded.
        int EXPIRED = 2;
    }

    /** Creates an instance of AiAssistantService. */
    private AiAssistantService() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        var providerFactory = ServiceLoaderUtil.maybeCreate(SystemAiProviderFactory.class);
        if (providerFactory != null) {
            mSystemAiProvider = providerFactory.createSystemAiProvider();
        } else {
            mSystemAiProvider = new SystemAiProviderUpstreamImpl();
        }
    }

    /**
     * Gets a singleton instance of AiAssistantService. TODO(404352265): Consider adding per-profile
     * instances for caching.
     *
     * @return An instance of AiAssistantService.
     */
    public static AiAssistantService getInstance() {
        if (sInstance == null) {
            sInstance = new AiAssistantService();
        }

        return sInstance;
    }

    /** Clear out the singleton instance for testing. */
    public static void resetForTesting() {
        sInstance = null;
    }

    /**
     * Sets an instance of AiAssistantService for testing.
     *
     * @param aiAssistantService An instance of AiAssistantService to replace the singleton instance
     *     with.
     */
    public static void setInstanceForTesting(AiAssistantService aiAssistantService) {
        var previousInstance = sInstance;
        sInstance = aiAssistantService;
        ResettersForTesting.register(() -> sInstance = previousInstance);
    }

    /**
     * Shows an AI assistant UI for the current tab. canShowAiForTab should be called before.
     *
     * @param context Current activity.
     * @param tab Current tab.
     */
    public void showAi(Context context, Tab tab) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)) {
            return;
        }
        assert mIsInitialized;

        if (!isTabElegible(tab)) return;

        refreshAvailability(
                context,
                tab.getProfile(),
                () -> {
                    requestLaunch(context, tab);
                });
    }

    /**
     * Checks if the AI Assistant UI can be shown for a specific tab,
     *
     * @param context Context used to query the system provider
     * @param tab Tab to check
     * @return True if the tab is eligible for the AI assistant UI, false otherwise.
     */
    public boolean canShowAiForTab(Context context, Tab tab) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)) {
            return false;
        }

        if (!mIsInitialized) {
            // Request availability.
            var initialized = initialize(context, tab.getProfile());
            // If after requesting we are still not initialized that means that we queried the
            // system service, return false as it's an async process.
            if (!initialized) {
                return false;
            }
        }

        if (!isTabElegible(tab)) {
            return false;
        } else if (isTabWebPage(tab) && mIsSummarizeAvailable) {
            return true;
        } else if (isTabPdf(tab) && mIsAnalyzeAttachmentAvailable) {
            return true;
        }

        return false;
    }

    /**
     * Initializes the service.
     *
     * @param context Context used to query the system provider
     * @param tab Tab used to get the profile
     * @return True if the service is initialized, false otherwise.
     */
    private boolean initialize(Context context, Profile profile) {
        if (mIsInitializing) {
            return true;
        }
        mIsInitializing = true;

        // First try to load cached availability from prefs.
        var loadingResult = tryLoadingAvailabilityFromPrefs();
        switch (loadingResult) {
            case PrefLoadingResult.LOADED -> {
                mIsInitializing = false;
                mIsInitialized = true;
                return true;
            }
            case PrefLoadingResult.NOT_AVAILABLE -> {
                // Request availability from system provider if cache is empty.
                refreshAvailability(context, profile, () -> {});
                return false;
            }
            case PrefLoadingResult.EXPIRED -> {
                mIsInitializing = false;
                mIsInitialized = true;
                // If the cache is expired then use its results one last time, and query the
                // provider to refresh the results in the background.
                refreshAvailability(context, profile, () -> {});
                return true;
            }
            default -> throw new IllegalStateException(
                    "Invalid PrefLoadingResult value: " + loadingResult);
        }
    }

    private void refreshAvailability(
            Context context, Profile profile, Runnable finishRefreshCallback) {
        shouldEnableForAccount(
                profile,
                (shouldEnable) -> {
                    if (shouldEnable) {
                        requestAvailabilityFromService(context, profile, finishRefreshCallback);
                    } else {
                        onAvailabilityResponse(null);
                        finishRefreshCallback.run();
                    }
                });
    }

    private void requestAvailabilityFromService(
            Context context, Profile profile, Runnable finishRefreshCallback) {
        var availabilityRequestBuilder =
                AvailabilityRequest.newBuilder()
                        .addRequestedCapabilities(Capability.ANALYZE_ATTACHMENT_CAPABILITY)
                        .addRequestedCapabilities(Capability.SUMMARIZE_URL_CAPABILITY);

        if (shouldAttachClientInfo()) {
            var accountEmail = getAccountEmail(profile);
            if (accountEmail != null) {
                availabilityRequestBuilder.setClientInfo(getClientInfo(accountEmail));
            }
        }

        var availabilityFuture =
                mSystemAiProvider.isAvailable(context, availabilityRequestBuilder.build());
        Futures.addCallback(
                availabilityFuture,
                new FutureCallback<>() {
                    @Override
                    public void onSuccess(@Nullable AvailabilityResponse result) {
                        onAvailabilityResponse(result);
                        finishRefreshCallback.run();
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        onAvailabilityResponse(null);
                        finishRefreshCallback.run();
                    }
                },
                MoreExecutors.directExecutor());
    }

    private void onAvailabilityResponse(@Nullable AvailabilityResponse result) {
        mIsInitializing = false;
        mIsInitialized = true;

        mIsSystemAiProviderAvailable = result != null && result.hasAvailable();
        if (mIsSystemAiProviderAvailable && result != null) {
            mIsSummarizeAvailable =
                    result.getAvailable()
                            .getSupportedCapabilitiesList()
                            .contains(Capability.SUMMARIZE_URL_CAPABILITY);
            mIsAnalyzeAttachmentAvailable =
                    result.getAvailable()
                            .getSupportedCapabilitiesList()
                            .contains(Capability.ANALYZE_ATTACHMENT_CAPABILITY);
        } else {
            var shouldUseFallback = isIntentFallbackEnabled();
            mIsSummarizeAvailable = shouldUseFallback;
            mIsAnalyzeAttachmentAvailable = shouldUseFallback;
        }

        saveAvailabilityToPrefs();
    }

    private @PrefLoadingResult int tryLoadingAvailabilityFromPrefs() {
        if (!mSharedPreferencesManager.contains(
                ChromePreferenceKeys.AI_ASSISTANT_AVAILABILITY_CHECK_TIMESTAMP_MS)) {
            return PrefLoadingResult.NOT_AVAILABLE;
        }

        mIsAnalyzeAttachmentAvailable =
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.AI_ASSISTANT_ANALYZE_ATTACHMENT_AVAILABILITY, false);
        mIsSummarizeAvailable =
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.AI_ASSISTANT_WEB_SUMMARIZATION_AVAILABILITY, false);

        var lastCheckTimestampMs =
                mSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.AI_ASSISTANT_AVAILABILITY_CHECK_TIMESTAMP_MS);
        var timeSinceLastCheckMs = System.currentTimeMillis() - lastCheckTimestampMs;
        if (timeSinceLastCheckMs >= getAvailabilityCacheDurationMs()) {
            // If the cache is expired then delete its values from prefs, but keep them loaded in
            // memory.
            deleteAvailabilityFromPrefs();

            return PrefLoadingResult.EXPIRED;
        }

        return PrefLoadingResult.LOADED;
    }

    private boolean isIntentFallbackEnabled() {
        boolean intentFallback =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY,
                        "intent_fallback",
                        false);
        return intentFallback;
    }

    private long getAvailabilityCacheDurationMs() {
        var durationDays =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY,
                        "availability_cache_duration_days",
                        1);
        return Duration.ofDays(durationDays).toMillis();
    }

    private boolean shouldDisableForParentalControl() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY,
                "disable_for_parent_control",
                true);
    }

    private boolean shouldDisableForEnterprise() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY,
                "disable_for_enterprise",
                true);
    }

    private void saveAvailabilityToPrefs() {
        // Don't cache results if system provider is not available.
        if (!mIsInitialized || !mIsSystemAiProviderAvailable) {
            deleteAvailabilityFromPrefs();
            return;
        }

        mSharedPreferencesManager
                .getEditor()
                .putLong(
                        ChromePreferenceKeys.AI_ASSISTANT_AVAILABILITY_CHECK_TIMESTAMP_MS,
                        System.currentTimeMillis())
                .putBoolean(
                        ChromePreferenceKeys.AI_ASSISTANT_ANALYZE_ATTACHMENT_AVAILABILITY,
                        mIsAnalyzeAttachmentAvailable)
                .putBoolean(
                        ChromePreferenceKeys.AI_ASSISTANT_WEB_SUMMARIZATION_AVAILABILITY,
                        mIsSummarizeAvailable)
                .apply();
    }

    private void deleteAvailabilityFromPrefs() {
        mSharedPreferencesManager
                .getEditor()
                .remove(ChromePreferenceKeys.AI_ASSISTANT_ANALYZE_ATTACHMENT_AVAILABILITY)
                .remove(ChromePreferenceKeys.AI_ASSISTANT_AVAILABILITY_CHECK_TIMESTAMP_MS)
                .remove(ChromePreferenceKeys.AI_ASSISTANT_WEB_SUMMARIZATION_AVAILABILITY)
                .apply();
    }

    private void requestLaunch(Context context, Tab tab) {
        if (!isTabElegible(tab)) return;

        if (!mIsSystemAiProviderAvailable && !isIntentFallbackEnabled()) {
            ThreadUtils.postOnUiThread(
                    () -> {
                        Toast.makeText(
                                        context,
                                        R.string.ai_assistant_service_error_toast,
                                        Toast.LENGTH_LONG)
                                .show();
                    });
            return;
        }

        if (isTabPdf(tab)
                && tab.getNativePage() instanceof PdfPage pdfPage
                && mIsAnalyzeAttachmentAvailable) {
            sendLaunchRequest(
                    context,
                    getLaunchRequestForAnalyzeAttachment(tab, pdfPage),
                    /* shouldUseSystemProvider= */ mIsSystemAiProviderAvailable
                            && mIsAnalyzeAttachmentAvailable);
        } else if (isTabWebPage(tab) && mIsSummarizeAvailable) {
            ThreadUtils.postOnUiThread(
                    () -> {
                        if (tab.getWebContents() == null || tab.isDestroyed()) return;
                        var mainFrame = tab.getWebContents().getMainFrame();
                        var shouldUseSystemProvider =
                                mIsSystemAiProviderAvailable && mIsSummarizeAvailable;
                        InnerTextBridge.getInnerText(
                                mainFrame,
                                innerText -> {
                                    onInnerTextReceived(
                                            context, tab, shouldUseSystemProvider, innerText);
                                });
                    });
        } else {
            ThreadUtils.postOnUiThread(
                    () -> {
                        Toast.makeText(
                                        context,
                                        R.string.ai_assistant_service_error_toast,
                                        Toast.LENGTH_LONG)
                                .show();
                    });
        }
    }

    private boolean isTabElegible(Tab tab) {
        if (tab == null || tab.getUrl() == null || tab.isOffTheRecord()) return false;

        if (tab.getNativePage() instanceof PdfPage) return true;

        return UrlUtilities.isHttpOrHttps(tab.getUrl());
    }

    private void shouldEnableForAccount(Profile profile, Callback<Boolean> shouldEnable) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) {
            shouldEnable.onResult(false);
            return;
        }
        CoreAccountInfo accountInfo = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (accountInfo == null) {
            // Apply no account policy if no account is available, AI service can check its policy.
            shouldEnable.onResult(true);
            return;
        }
        AccountManagerFacadeProvider.getInstance()
                .getAccountCapabilities(accountInfo)
                .then(
                        capabilities -> {
                            boolean enabled = true;
                            if (shouldDisableForParentalControl()
                                    && capabilities.isSubjectToParentalControls()
                                            != Tribool.FALSE) {
                                enabled = false;
                            } else if (shouldDisableForEnterprise()
                                    && capabilities.isSubjectToEnterpriseFeatures()
                                            != Tribool.FALSE) {
                                enabled = false;
                            }
                            shouldEnable.onResult(enabled);
                        });
    }

    private void onInnerTextReceived(
            Context context, Tab tab, boolean shouldUseSystemProvider, @Nullable String innerText) {
        if (innerText == null) {
            Log.w(TAG, "Error while extracting page contents");
            return;
        }

        sendLaunchRequest(
                context, getLaunchRequestForSummarizeUrl(tab, innerText), shouldUseSystemProvider);
    }

    private boolean isTabPdf(Tab tab) {
        if (tab.getNativePage() instanceof PdfPage pdfPage) {
            return pdfPage.getUri() != null;
        }
        return false;
    }

    private boolean isTabWebPage(Tab tab) {
        return !isTabPdf(tab)
                && UrlUtilities.isHttpOrHttps(tab.getUrl())
                && tab.getWebContents() != null;
    }

    private boolean shouldAttachClientInfo() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY,
                "attach_client_info",
                false);
    }

    private LaunchRequest getLaunchRequestForAnalyzeAttachment(Tab tab, PdfPage pdfPage) {
        assert pdfPage.getUri() != null;
        var file =
                File.newBuilder()
                        .setUri(pdfPage.getUri().toString())
                        .setDisplayName(pdfPage.getTitle())
                        .setMimeType("application/pdf");
        var analyzeAttachment = AnalyzeAttachment.newBuilder().addFiles(file);
        var launchRequestBuilder =
                LaunchRequest.newBuilder().setAnalyzeAttachment(analyzeAttachment);
        maybeAttachClientInfoToLaunchRequest(launchRequestBuilder, tab.getProfile());

        return launchRequestBuilder.build();
    }

    private LaunchRequest getLaunchRequestForSummarizeUrl(Tab tab, String innerText) {
        var urlContext =
                UrlContext.newBuilder().setUrl(tab.getUrl().getSpec()).setPageContent(innerText);
        var summarizeUrl = SummarizeUrl.newBuilder().setUrlContext(urlContext);
        var launchRequestBuilder = LaunchRequest.newBuilder().setSummarizeUrl(summarizeUrl);
        maybeAttachClientInfoToLaunchRequest(launchRequestBuilder, tab.getProfile());

        return launchRequestBuilder.build();
    }

    private void maybeAttachClientInfoToLaunchRequest(
            LaunchRequest.Builder builder, Profile profile) {
        if (!shouldAttachClientInfo()) {
            return;
        }

        var accountEmail = getAccountEmail(profile);

        if (accountEmail == null) return;

        builder.setClientInfo(getClientInfo(accountEmail));
    }

    private ClientInfo getClientInfo(String accountEmail) {
        var account = Account.newBuilder();
        account.setEmail(accountEmail);
        var clientInfo = ClientInfo.newBuilder().addClientAccount(account);
        return clientInfo.build();
    }

    private @Nullable String getAccountEmail(Profile profile) {
        var identityManager = IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) return null;

        var accountInfo = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (accountInfo == null) return null;

        return accountInfo.getEmail();
    }

    private void sendLaunchRequest(
            Context context, LaunchRequest launchRequest, boolean shouldUseSystemProvider) {
        if (shouldUseSystemProvider) {
            sendLaunchRequestToSystemProvider(context, launchRequest);
        } else {
            sendLaunchRequestWithIntent(context, launchRequest);
        }
    }

    private void sendLaunchRequestToSystemProvider(Context context, LaunchRequest launchRequest) {
        mSystemAiProvider.launch(context, launchRequest);
    }

    private void sendLaunchRequestWithIntent(Context context, LaunchRequest launchRequest) {
        Log.w(TAG, "Unable to use to system AI provider, sending intent instead");
        Intent assistantIntent = new Intent(Intent.ACTION_VOICE_COMMAND);
        assistantIntent.putExtra(EXTRA_LAUNCH_REQUEST, launchRequest.toByteArray());
        try {
            context.startActivity(assistantIntent);
        } catch (ActivityNotFoundException ex) {
            // Ignore exception, as this is a fallback.
            Log.w(TAG, "Exception while trying to send ACTION_VOICE_COMMAND intent", ex);
        }
    }
}
