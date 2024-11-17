// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import androidx.annotation.IntDef;

import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;
import org.chromium.ui.base.ResourceBundle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * Singleton class to manage installing language splits. This is temporary to start testing Play
 * Store downloads. |SplitCompatEngine| should be modified to support language split installs,
 * https://crbug.com/1186903, or a test suite should be added to this class.
 */
public class LanguageSplitInstaller {
    private static LanguageSplitInstaller sLanguageSplitInstaller;
    private static final String TAG = "LanguageInstaller";

    // Constants used to log UMA enum histogram, must stay in sync with
    // LanguageSettingsSplitInstallStatus from enums.xml
    @IntDef({
        LanguageSplitInstallStatus.SUCCESS,
        LanguageSplitInstallStatus.ALREADY_INSTALLED,
        LanguageSplitInstallStatus.CANCELED,
        LanguageSplitInstallStatus.DOWNLOADED,
        LanguageSplitInstallStatus.FAILED,
        LanguageSplitInstallStatus.UNEXPECTED_STATUS
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface LanguageSplitInstallStatus {
        int SUCCESS = 0;
        int ALREADY_INSTALLED = 1;
        int CANCELED = 2;
        int DOWNLOADED = 3;
        int FAILED = 4;
        int UNEXPECTED_STATUS = 5;
        int NUM_ENTRIES = 6;
    }

    /** Broadcast listener for language split downloads. */
    public interface InstallListener {
        /**
         * Called when the language split install is completed.
         * @param success True if the module was installed successfully.
         */
        void onComplete(boolean success);
    }

    private final SplitInstallStateUpdatedListener mStateUpdateListener = getStatusUpdateListener();
    private InstallListener mInstallListener;
    private SplitInstallManager mSplitInstallManager;
    private int mInstallSessionId;
    private boolean mIsLanguageSplitInstalled;

    private LanguageSplitInstaller() {
        mSplitInstallManager =
                SplitInstallManagerFactory.create(ContextUtils.getApplicationContext());
    }

    /**
     * Get the set of installed languages as language code strings.
     *
     * @return Set<String> of installed languages code strings.
     */
    public Set<String> getInstalledLanguages() {
        // On non-bundle builds return all packaged locales.
        if (!BuildConfig.IS_BUNDLE) {
            return new HashSet<String>(Arrays.asList(ResourceBundle.getAvailableLocales()));
        }
        return mSplitInstallManager.getInstalledLanguages();
    }

    /**
     * Return true if the language pack for language name is installed. By definition the value to
     * follow the system language is always installed.
     * @param languageName BCP-47 language code.
     * @return True if split for |languageName| is installed.
     */
    public boolean isLanguageSplitInstalled(String languageName) {
        return getInstalledLanguages().contains(languageName)
                || AppLocaleUtils.isFollowSystemLanguage(languageName);
    }

    /**
     * Start the install of a language split for |languageName| and use |listener| as a callback
     * for when the install is completed or has failed. Note: The API instantly considers the
     * request as completed if it detects the language is already installed and the Play Store
     * will automatically update the language split in the background.
     * @param languageName String BCP-47 code for language to be installed.
     * @param InstallListener Callback to handle install success or failure.
     */
    public void installLanguage(String languageName, InstallListener listener) {
        if (mInstallListener != null) {
            throw new UnsupportedOperationException(
                    "Only supports one language install at a time.");
        }

        mInstallListener = listener;
        mSplitInstallManager.registerListener(mStateUpdateListener);
        Locale installLocale = Locale.forLanguageTag(languageName);

        SplitInstallRequest installRequest =
                SplitInstallRequest.newBuilder().addLanguage(installLocale).build();

        mIsLanguageSplitInstalled = isLanguageSplitInstalled(languageName);
        mSplitInstallManager
                .startInstall(installRequest)
                .addOnSuccessListener(
                        sessionId -> {
                            mInstallSessionId = sessionId;
                        })
                .addOnFailureListener(
                        exception -> {
                            Log.i(TAG, "Language Split Failure:", exception);
                            installFinished(false);
                        });

        // Schedule a deferred install if the live install fails the play store will install the
        // language split in the background at the next hygiene run. If the install succeeds the
        // deferred install is ignored at the next hygiene run.
        mSplitInstallManager.deferredLanguageInstall(Arrays.asList(installLocale));
    }

    /**
     * Run cleanup and call install listener when the install has finished.
     * @param success True if the install was successful.
     */
    private void installFinished(boolean success) {
        mInstallListener.onComplete(success);
        mSplitInstallManager.unregisterListener(mStateUpdateListener);
        mInstallListener = null;
        mInstallSessionId = 0;
        mIsLanguageSplitInstalled = false;
    }

    /**
     * @param status SplitInstallSessionStatus
     * @return True if status is a final state for the split install. |SplitInstallSessionStatus|
     * has values for intermediate states such as Pending and Installing which are not final states.
     */
    private boolean isStatusFinalState(@SplitInstallSessionStatus int status) {
        if (status == SplitInstallSessionStatus.INSTALLED
                || status == SplitInstallSessionStatus.FAILED
                || status == SplitInstallSessionStatus.CANCELED
                || status == SplitInstallSessionStatus.DOWNLOADED) {
            return true;
        }
        return false;
    }

    /**
     * Make a SplitInstallStateUpdateListener that responds once the download has been installed or
     * fails.
     */
    private SplitInstallStateUpdatedListener getStatusUpdateListener() {
        return state -> { // Lambda for SplitInstallStatusUpdateListener.onStateUpdate.
            if (state.sessionId() != mInstallSessionId) return;

            int status = state.status();
            if (isStatusFinalState(status)) {
                recordLanguageSplitInstallStatus(status);
                installFinished(status == SplitInstallSessionStatus.INSTALLED);
            }
        };
    }

    private @LanguageSplitInstallStatus int getEnumCodeFromStatus(
            @SplitInstallSessionStatus int status) {
        switch (status) {
            case SplitInstallSessionStatus.INSTALLED:
                return mIsLanguageSplitInstalled
                        ? LanguageSplitInstallStatus.ALREADY_INSTALLED
                        : LanguageSplitInstallStatus.SUCCESS;
            case SplitInstallSessionStatus.CANCELED:
                return LanguageSplitInstallStatus.CANCELED;
            case SplitInstallSessionStatus.DOWNLOADED:
                return LanguageSplitInstallStatus.DOWNLOADED;
            case SplitInstallSessionStatus.FAILED:
                return LanguageSplitInstallStatus.FAILED;
            default:
                return LanguageSplitInstallStatus.UNEXPECTED_STATUS;
        }
    }

    private void recordLanguageSplitInstallStatus(@SplitInstallSessionStatus int status) {
        @LanguageSplitInstallStatus int enumCode = getEnumCodeFromStatus(status);
        RecordHistogram.recordEnumeratedHistogram(
                "LanguageSettings.SplitInstallFinalStatus",
                enumCode,
                LanguageSplitInstallStatus.NUM_ENTRIES);
    }

    /**
     * @return Singleton instance of LanguageSplitInstaller
     */
    public static LanguageSplitInstaller getInstance() {
        if (sLanguageSplitInstaller == null) {
            sLanguageSplitInstaller = new LanguageSplitInstaller();
        }
        return sLanguageSplitInstaller;
    }
}
