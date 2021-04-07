// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.util.Arrays;
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

    /**
     * Broadcast listener for language split downloads.
     */
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

    private LanguageSplitInstaller() {
        mSplitInstallManager =
                SplitInstallManagerFactory.create(ContextUtils.getApplicationContext());
    }

    /**
     * Get the set of installed languages as language code strings.
     * @return Set<String> of installed languages code strings.
     */
    public Set<String> getInstalledLanguages() {
        return mSplitInstallManager.getInstalledLanguages();
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

        mSplitInstallManager.startInstall(installRequest)
                .addOnSuccessListener(sessionId -> { mInstallSessionId = sessionId; })
                .addOnFailureListener(exception -> {
                    Log.i(TAG, "Language Split Failure:", exception);
                    listener.onComplete(false);
                });

        // Schedule a deferred install if the live install fails the play store will install the
        // language split in the background at the next hygiene run. If the install succeeds the
        // deferred install is ignored at the next hygiene run.
        mSplitInstallManager.deferredLanguageInstall(Arrays.asList(installLocale));
    }

    /**
     * Make a SplitInstallStateUpdateListener that responds once the download has been installed or
     * fails.
     */
    private SplitInstallStateUpdatedListener getStatusUpdateListener() {
        return state -> { // Lambda for SplitInstallStatusUpdateListener.onStateUpdate.
            if (state.sessionId() != mInstallSessionId) return;

            int status = state.status();
            if (status == SplitInstallSessionStatus.INSTALLED
                    || status == SplitInstallSessionStatus.FAILED) {
                mInstallListener.onComplete(status == SplitInstallSessionStatus.INSTALLED);
                mSplitInstallManager.unregisterListener(mStateUpdateListener);
                mInstallListener = null;
                mInstallSessionId = 0;
            }
        };
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
