// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * The activity that host post-UNO sign-in flows. This activity is semi-transparent, and views for
 * different sign-in flow steps will be hosted by it, according to the account's state and the flow
 * type.
 *
 * <p>For most cases, the flow is contains a sign-in bottom sheet, and a history sync opt-in dialog
 * shown after sign-in completion.
 */
public class SigninAndHistoryOptInActivity extends AsyncInitializationActivity
        implements SigninAndHistoryOptInCoordinator.Delegate {
    private static final String ARGUMENT_ACCESS_POINT = "SigninAndHistoryOptInActivity.AccessPoint";
    private static final String ARGUMENT_NO_ACCOUNT_SIGNIN_MODE =
            "SigninAndHistoryOptInActivity.NoAccountSigninMode";
    private static final String ARGUMENT_HISTORY_OPT_IN_MODE =
            "SigninAndHistoryOptInActivity.HistoryOptInMode";

    private OneshotSupplierImpl<Profile> mProfileSupplier = new OneshotSupplierImpl<>();
    private SigninAndHistoryOptInCoordinator mCoordinator;

    @Override
    protected void onPreCreate() {
        // Temporarily ensure that the native is initialized before calling super.onCreate().
        // TODO(https://crbug.com/1520783): Handle the case where the UI is shown before the end of
        // native initialization.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
    }

    @Override
    protected void triggerLayoutInflation() {
        Intent intent = getIntent();
        int signinAccessPoint = intent.getIntExtra(ARGUMENT_ACCESS_POINT, SigninAccessPoint.MAX);
        assert signinAccessPoint != SigninAccessPoint.MAX : "Cannot find SigninAccessPoint!";
        @SigninAndHistoryOptInCoordinator.NoAccountSigninMode
        int noAccountSigninMode =
                intent.getIntExtra(
                        ARGUMENT_NO_ACCOUNT_SIGNIN_MODE,
                        SigninAndHistoryOptInCoordinator.NoAccountSigninMode.ADD_ACCOUNT);
        @SigninAndHistoryOptInCoordinator.HistoryOptInMode
        int historyOptInMode =
                intent.getIntExtra(
                        ARGUMENT_HISTORY_OPT_IN_MODE,
                        SigninAndHistoryOptInCoordinator.HistoryOptInMode.OPTIONAL);

        mCoordinator =
                new SigninAndHistoryOptInCoordinator(
                        getWindowAndroid(),
                        this,
                        this,
                        DeviceLockActivityLauncherImpl.get(),
                        mProfileSupplier,
                        getModalDialogManagerSupplier(),
                        noAccountSigninMode,
                        historyOptInMode,
                        signinAccessPoint);

        setContentView(mCoordinator.getView());
        onInitialLayoutInflationComplete();
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        ActivityProfileProvider profileProvider =
                new ActivityProfileProvider(getLifecycleDispatcher()) {
                    @Nullable
                    @Override
                    protected OTRProfileID createOffTheRecordProfileID() {
                        throw new IllegalStateException(
                                "Attempting to access incognito in the sign-in & history sync"
                                        + " opt-in flow");
                    }
                };

        profileProvider.onAvailable(
                (provider) -> {
                    mProfileSupplier.set(profileProvider.get().getOriginalProfile());
                });
        return profileProvider;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this, /* listenToActivityState= */ true, getIntentRequestTracker());
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    public void onFlowComplete() {
        finish();
        // Remove activity animation to avoid visual glitches due to the semi-transparent
        // background.
        overridePendingTransition(0, R.anim.no_anim);
    }

    @Override
    protected void onDestroy() {
        mCoordinator.destroy();
        super.onDestroy();
    }

    public static @NonNull Intent createIntent(
            Context context,
            @SigninAndHistoryOptInCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @SigninAndHistoryOptInCoordinator.HistoryOptInMode int historyOptInMode,
            @SigninAccessPoint int signinAccessPoint) {
        Intent intent = new Intent(context, SigninAndHistoryOptInActivity.class);
        intent.putExtra(ARGUMENT_NO_ACCOUNT_SIGNIN_MODE, noAccountSigninMode);
        intent.putExtra(ARGUMENT_HISTORY_OPT_IN_MODE, historyOptInMode);
        intent.putExtra(ARGUMENT_ACCESS_POINT, signinAccessPoint);
        return intent;
    }
}
