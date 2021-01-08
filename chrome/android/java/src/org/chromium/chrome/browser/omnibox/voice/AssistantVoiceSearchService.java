// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_LAST_VERSION;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_SUPPORTED;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Service for state tracking and event delivery to classes that need to observe the state
 * of Assistant Voice Search.
 **/
public class AssistantVoiceSearchService implements TemplateUrlService.TemplateUrlServiceObserver,
                                                    GSAState.Observer, ProfileManager.Observer {
    private static final String USER_ELIGIBILITY_HISTOGRAM =
            "Assistant.VoiceSearch.UserEligibility";
    @VisibleForTesting
    static final String USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM =
            "Assistant.VoiceSearch.UserEligibility.FailureReason";
    private static final String DEFAULT_ASSISTANT_AGSA_MIN_VERSION = "11.7";
    private static final boolean DEFAULT_ASSISTANT_COLORFUL_MIC_ENABLED = false;

    // Cached value for expensive content provider read.
    // True - Value has been read and is true.
    // False - Value has been read and is false.
    // Null - Value hasn't been read.
    private static Boolean sAgsaSupportsAssistantVoiceSearch;

    @IntDef({EligibilityFailureReason.AGSA_CANT_HANDLE_INTENT,
            EligibilityFailureReason.AGSA_VERSION_BELOW_MINIMUM,
            EligibilityFailureReason.AGSA_DOESNT_SUPPORT_VOICE_SEARCH_CHECK_NOT_COMPLETE,
            EligibilityFailureReason.AGSA_DOESNT_SUPPORT_VOICE_SEARCH,
            EligibilityFailureReason.CHROME_NOT_GOOGLE_SIGNED,
            EligibilityFailureReason.AGSA_NOT_GOOGLE_SIGNED,
            EligibilityFailureReason.ACCOUNT_MISMATCH, EligibilityFailureReason.MAX_VALUE})
    @Retention(RetentionPolicy.SOURCE)
    @interface EligibilityFailureReason {
        int AGSA_CANT_HANDLE_INTENT = 0;
        int AGSA_VERSION_BELOW_MINIMUM = 1;
        int AGSA_DOESNT_SUPPORT_VOICE_SEARCH_CHECK_NOT_COMPLETE = 2;
        int AGSA_DOESNT_SUPPORT_VOICE_SEARCH = 3;
        int CHROME_NOT_GOOGLE_SIGNED = 4;
        int AGSA_NOT_GOOGLE_SIGNED = 5;
        int ACCOUNT_MISMATCH = 6;

        // STOP: When updating this, also update values in enums.xml.
        int MAX_VALUE = 7;
    }

    /** Allows outside classes to listen for changes in this service. */
    public interface Observer {
        /**
         * Called when the service changes, use relevant getters to update your state. This
         * indicates that the state of AssistantVoiceSearchService has changed and you should
         * re-query the getters you're interested in.
         * - The microphone Drawable icon can change during runtime.
         **/
        void onAssistantVoiceSearchServiceChanged();
    }

    // TODO(wylieb): Convert this to an ObserverList and add #addObserver, #removeObserver.
    private final Observer mObserver;
    private final Context mContext;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final TemplateUrlService mTemplateUrlService;
    private final GSAState mGsaState;
    private final SharedPreferencesManager mSharedPrefsManager;

    private boolean mIsDefaultSearchEngineGoogle;
    private boolean mIsAssistantVoiceSearchEnabled;
    private boolean mIsColorfulMicEnabled;
    private boolean mShouldShowColorfulMic;
    private String mMinAgsaVersion;

    public AssistantVoiceSearchService(@NonNull Context context,
            @NonNull ExternalAuthUtils externalAuthUtils,
            @NonNull TemplateUrlService templateUrlService, @NonNull GSAState gsaState,
            @Nullable Observer observer, @NonNull SharedPreferencesManager sharedPrefsManager) {
        mContext = context;
        mExternalAuthUtils = externalAuthUtils;
        mTemplateUrlService = templateUrlService;
        mGsaState = gsaState;
        mSharedPrefsManager = sharedPrefsManager;
        mObserver = observer;

        ProfileManager.addObserver(this);
        mGsaState.addObserver(this);
        mTemplateUrlService.addObserver(this);
        initializeAssistantVoiceSearchState();
    }

    public void destroy() {
        mTemplateUrlService.removeObserver(this);
        mGsaState.removeObserver(this);
        ProfileManager.removeObserver(this);
    }

    private void notifyObserver() {
        if (mObserver == null) return;
        mObserver.onAssistantVoiceSearchServiceChanged();
    }

    /** Cache Assistant voice search variable state. */
    @VisibleForTesting
    void initializeAssistantVoiceSearchState() {
        mIsAssistantVoiceSearchEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);

        mIsColorfulMicEnabled = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "colorful_mic",
                DEFAULT_ASSISTANT_COLORFUL_MIC_ENABLED);

        mMinAgsaVersion = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, "min_agsa_version");

        mIsDefaultSearchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();

        mShouldShowColorfulMic = isColorfulMicEnabled();

        // Baseline conditions to avoid doing the expensive query to a content provider.
        if (mIsAssistantVoiceSearchEnabled && mIsDefaultSearchEngineGoogle
                && !SysUtils.isLowEndDevice()) {
            checkIfAssistantEnabled();
        } else {
            sAgsaSupportsAssistantVoiceSearch = false;
        }
    }

    /** @return Whether the user has had a chance to enable the feature. */
    public boolean needsEnabledCheck() {
        return !mSharedPrefsManager.contains(ASSISTANT_VOICE_SEARCH_ENABLED);
    }

    /**
     * Checks if the client is eligible Assistant for voice search. It's
     * {@link canRequestAssistantVoiceSearch} with an additional check for experiment groups.
     */
    public boolean shouldRequestAssistantVoiceSearch() {
        return mIsAssistantVoiceSearchEnabled && canRequestAssistantVoiceSearch()
                && isEnabledByPreference();
    }

    /** Checks if the client meetings the requirements to use Assistant for voice search. */
    public boolean canRequestAssistantVoiceSearch() {
        return mIsDefaultSearchEngineGoogle && isDeviceEligibleForAssistant();
    }

    /**
     * @return Gets the current mic drawable, this will create the drawable if it doesn't already
     *         exist or reuse the existing Drawable's ConstantState if it does already exist.
     **/
    public Drawable getCurrentMicDrawable() {
        return AppCompatResources.getDrawable(
                mContext, mShouldShowColorfulMic ? R.drawable.ic_colorful_mic : R.drawable.btn_mic);
    }

    /** @return The correct ColorStateList for the current theme. */
    public @Nullable ColorStateList getMicButtonColorStateList(
            @ColorInt int primaryColor, Context context) {
        if (mShouldShowColorfulMic) return null;

        final boolean useLightColors =
                ColorUtils.shouldUseLightForegroundOnBackground(primaryColor);
        int id = ChromeColors.getPrimaryIconTintRes(useLightColors);
        return AppCompatResources.getColorStateList(context, id);
    }

    /** Called from {@link VoiceRecognitionHandler} after the consent flow has completed. */
    public void onAssistantConsentDialogComplete(boolean useAssistant) {
        if (useAssistant) updateColorfulMicState();
    }

    /**
     * @return Whether the user has enabled the feature, ensure {@link needsEnabledCheck} is
     *         called first.
     */
    private boolean isEnabledByPreference() {
        return mSharedPrefsManager.readBoolean(
                ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false);
    }

    /** Does expensive content provider read to determine if AGSA supports Assistant. */
    private void checkIfAssistantEnabled() {
        final String currentAgsaVersion = mGsaState.getAgsaVersionName();
        if (mSharedPrefsManager.contains(ASSISTANT_VOICE_SEARCH_SUPPORTED)
                && mSharedPrefsManager
                           .readString(ASSISTANT_LAST_VERSION,
                                   /* default= */ "n/a")
                           .equals(currentAgsaVersion)) {
            sAgsaSupportsAssistantVoiceSearch =
                    mSharedPrefsManager.readBoolean(ASSISTANT_VOICE_SEARCH_SUPPORTED,
                            /* default= */ false);
            updateColorfulMicState();
        } else {
            DeferredStartupHandler.getInstance().addDeferredTask(() -> {
                // Only do this once per browser start.
                if (sAgsaSupportsAssistantVoiceSearch != null) return;
                sAgsaSupportsAssistantVoiceSearch = false;
                new AsyncTask<Boolean>() {
                    @Override
                    public Boolean doInBackground() {
                        return mGsaState.agsaSupportsAssistantVoiceSearch();
                    }

                    @Override
                    public void onPostExecute(Boolean agsaSupportsAssistantVoiceSearch) {
                        sAgsaSupportsAssistantVoiceSearch = agsaSupportsAssistantVoiceSearch;
                        mSharedPrefsManager.writeBoolean(ASSISTANT_VOICE_SEARCH_SUPPORTED,
                                sAgsaSupportsAssistantVoiceSearch);
                        mSharedPrefsManager.writeString(
                                ASSISTANT_LAST_VERSION, currentAgsaVersion);
                        updateColorfulMicState();
                    }
                }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            });
        }
    }

    /** @return Whether the colorful mic is enabled. */
    private boolean isColorfulMicEnabled() {
        return mContext.getPackageManager() != null && mIsColorfulMicEnabled
                && shouldRequestAssistantVoiceSearch();
    }

    /**
     * @return The min Agsa version required for assistant voice search. This method checks the
     *         chrome feature list for this value before using the default.
     */
    private String getAgsaMinVersion() {
        if (TextUtils.isEmpty(mMinAgsaVersion)) return DEFAULT_ASSISTANT_AGSA_MIN_VERSION;
        return mMinAgsaVersion;
    }

    /** @return Whether the device is eligible to use assistant. */
    @VisibleForTesting
    protected boolean isDeviceEligibleForAssistant() {
        if (!mGsaState.canAgsaHandleIntent(getAssistantVoiceSearchIntent())) {
            RecordHistogram.recordEnumeratedHistogram(USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                    EligibilityFailureReason.AGSA_CANT_HANDLE_INTENT,
                    EligibilityFailureReason.MAX_VALUE);
            return false;
        }

        if (mGsaState.isAgsaVersionBelowMinimum(
                    mGsaState.getAgsaVersionName(), getAgsaMinVersion())) {
            RecordHistogram.recordEnumeratedHistogram(USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                    EligibilityFailureReason.AGSA_VERSION_BELOW_MINIMUM,
                    EligibilityFailureReason.MAX_VALUE);
            return false;
        }

        // Query AGSA to see if it can handle Assistant voice search.
        if (sAgsaSupportsAssistantVoiceSearch == null || !sAgsaSupportsAssistantVoiceSearch) {
            RecordHistogram.recordEnumeratedHistogram(USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                    sAgsaSupportsAssistantVoiceSearch == null
                            ? EligibilityFailureReason
                                      .AGSA_DOESNT_SUPPORT_VOICE_SEARCH_CHECK_NOT_COMPLETE
                            : EligibilityFailureReason.AGSA_DOESNT_SUPPORT_VOICE_SEARCH,
                    EligibilityFailureReason.MAX_VALUE);
            return false;
        }

        // AGSA will throw an exception if Chrome isn't Google signed.
        if (!mExternalAuthUtils.isChromeGoogleSigned()) {
            RecordHistogram.recordEnumeratedHistogram(USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                    EligibilityFailureReason.CHROME_NOT_GOOGLE_SIGNED,
                    EligibilityFailureReason.MAX_VALUE);
            return false;
        }
        if (!mExternalAuthUtils.isGoogleSigned(IntentHandler.PACKAGE_GSA)) {
            RecordHistogram.recordEnumeratedHistogram(USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                    EligibilityFailureReason.AGSA_NOT_GOOGLE_SIGNED,
                    EligibilityFailureReason.MAX_VALUE);
            return false;
        }

        if (!mGsaState.doesGsaAccountMatchChrome()) {
            RecordHistogram.recordEnumeratedHistogram(USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                    EligibilityFailureReason.ACCOUNT_MISMATCH, EligibilityFailureReason.MAX_VALUE);
            return false;
        }

        return true;
    }

    /** @return The Intent for Assistant voice search. */
    public Intent getAssistantVoiceSearchIntent() {
        Intent intent = new Intent(Intent.ACTION_SEARCH);
        intent.setPackage(IntentHandler.PACKAGE_GSA);
        return intent;
    }

    /** Records whether the user is eligible. */
    void reportUserEligibility() {
        RecordHistogram.recordBooleanHistogram(
                USER_ELIGIBILITY_HISTOGRAM, canRequestAssistantVoiceSearch());
    }

    private void updateColorfulMicState() {
        final boolean shouldShowColorfulMic = isColorfulMicEnabled();
        // Execute the update/notification in an AsyncTask to prevent re-entrant calls.
        new AsyncTask<Boolean>() {
            @Override
            protected Boolean doInBackground() {
                return mShouldShowColorfulMic != shouldShowColorfulMic;
            }
            @Override
            protected void onPostExecute(Boolean notify) {
                mShouldShowColorfulMic = shouldShowColorfulMic;
                if (notify) notifyObserver();
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    // TemplateUrlService.TemplateUrlServiceObserver implementation

    @Override
    public void onTemplateURLServiceChanged() {
        boolean searchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (mIsDefaultSearchEngineGoogle == searchEngineGoogle) return;

        mIsDefaultSearchEngineGoogle = searchEngineGoogle;
        mShouldShowColorfulMic = isColorfulMicEnabled();
        notifyObserver();
    }

    // GSAState.Observer implementation

    @Override
    public void onSetGsaAccount() {
        updateColorfulMicState();
    }

    // ProfileManager.Observer implementation

    @Override
    public void onProfileAdded(Profile profile) {
        updateColorfulMicState();
    }

    @Override
    public void onProfileDestroyed(Profile profile) {}

    // Test-only methods

    /** Enable the colorful mic for testing purposes. */
    void setColorfulMicEnabledForTesting(boolean enabled) {
        mIsColorfulMicEnabled = enabled;
        mShouldShowColorfulMic = enabled;
    }

    /** Allows skipping the cross-app check for testing. */
    public static void setAgsaSupportsAssistantVoiceSearchForTesting(Boolean value) {
        sAgsaSupportsAssistantVoiceSearch = value;
    }
}
