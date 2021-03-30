// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;
import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.util.ArrayList;
import java.util.List;

/**
 * Util methods for signin metrics logging.
 */
public class SigninMetricsUtils {
    /**
     * Scheduler class to poll native side every 15 seconds for 8 times to log web signin event.
     * Holds a single instance of the class that is reset between logWebSignin() calls if
     * a Scheduler object is already running.
     */
    private static class Scheduler {
        private static final int MAX_COUNT = 8;
        private static final int DELAY = 15 * 1000; // 15 seconds

        private static Scheduler sInstance;

        private int mCounter;
        private String[] mGaiaIds;
        private final Handler mHandler = new Handler();
        private final Runnable mPeriodicPoll = new Runnable() {
            @Override
            public void run() {
                if (mCounter < MAX_COUNT && !SigninMetricsUtilsJni.get().logWebSignin(mGaiaIds)) {
                    mCounter++;
                    postTask();
                }
            }
        };

        private void postTask() {
            mHandler.postDelayed(mPeriodicPoll, DELAY);
        }

        private static void logWebSignin(String[] gaiaIds) {
            if (sInstance == null) {
                sInstance = new Scheduler();
            }
            sInstance.mHandler.removeCallbacks(sInstance.mPeriodicPoll);
            sInstance.mCounter = 0;
            sInstance.mGaiaIds = gaiaIds;
            sInstance.postTask();
        }
    }

    /**
     * Logs a {@link ProfileAccountManagementMetrics} for a given {@link GAIAServiceType}.
     */
    public static void logProfileAccountManagementMenu(
            @ProfileAccountManagementMetrics int metric, @GAIAServiceType int gaiaServiceType) {
        SigninMetricsUtilsJni.get().logProfileAccountManagementMenu(metric, gaiaServiceType);
    }

    /**
     * Logs Signin.AccountConsistencyPromoAction histogram.
     */
    public static void logAccountConsistencyPromoAction(
            @AccountConsistencyPromoAction int promoAction) {
        RecordHistogram.recordEnumeratedHistogram("Signin.AccountConsistencyPromoAction",
                promoAction, AccountConsistencyPromoAction.MAX);
    }

    /**
     * Logs AccountPickerBottomSheet shown count histograms.
     */
    public static void logAccountConsistencyPromoShownCount(String histogram) {
        RecordHistogram.recordExactLinearHistogram(histogram,
                SigninPreferencesManager.getInstance().getAccountPickerBottomSheetShownCount(),
                100);
    }

    /**
     * Logs the access point when the user see the view of choosing account to sign in. Sign-in
     * completion histogram is recorded by {@link SigninManager#signinAndEnableSync}.
     *
     * @param accessPoint {@link SigninAccessPoint} that initiated the sign-in flow.
     */
    public static void logSigninStartAccessPoint(@SigninAccessPoint int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninStartedAccessPoint", accessPoint, SigninAccessPoint.MAX);
    }

    /**
     * Logs signin user action for a given {@link SigninAccessPoint}.
     */
    public static void logSigninUserActionForAccessPoint(@SigninAccessPoint int accessPoint) {
        SigninMetricsUtilsJni.get().logSigninUserActionForAccessPoint(accessPoint);
    }

    /**
     * Logs metrics when the user signs in within 2 minutes after dismissing the bottom sheet.
     * Polls the native side every 15 seconds for web signin events for 2 minutes.
     */
    public static void logWebSignin() {
        new AsyncTask<List<String>>() {
            @Override
            protected List<String> doInBackground() {
                AccountManagerFacade accountManagerFacade =
                        AccountManagerFacadeProvider.getInstance();
                List<Account> accounts = accountManagerFacade.tryGetGoogleAccounts();
                List<String> gaiaIds = new ArrayList<>();
                for (Account account : accounts) {
                    String gaiaId = accountManagerFacade.getAccountGaiaId(account.name);
                    if (gaiaId != null) {
                        gaiaIds.add(gaiaId);
                    }
                }
                return gaiaIds;
            }

            @Override
            protected void onPostExecute(List<String> gaiaIds) {
                Scheduler.logWebSignin(gaiaIds.toArray(new String[0]));
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void logProfileAccountManagementMenu(int metric, int gaiaServiceType);
        void logSigninUserActionForAccessPoint(int accessPoint);
        // Returns whether metrics were recorded on the native side.
        boolean logWebSignin(String[] gaiaIds);
    }

    private SigninMetricsUtils() {}
}
