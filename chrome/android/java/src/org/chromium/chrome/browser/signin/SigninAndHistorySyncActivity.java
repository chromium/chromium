// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;
import android.view.Window;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.firstrun.FirstRunActivityBase;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.DialogWhenLargeContentLayout;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.UiUtils;
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
 *
 * <p>The activity may also hold the re-FRE which consists of a fullscreen sign-in dialog followed
 * by the history sync opt-in. This is why the dependency on {@link
 * FullscreenSigninAndHistorySyncActivityBase} is needed.
 */
public class SigninAndHistorySyncActivity extends FullscreenSigninAndHistorySyncActivityBase
        implements BottomSheetSigninAndHistorySyncCoordinator.Delegate,
                FullscreenSigninAndHistorySyncCoordinator.Delegate {
    private static final String ARGUMENT_ACCESS_POINT = "SigninAndHistorySyncActivity.AccessPoint";
    private static final String ARGUMENT_IS_FULLSCREEN_SIGNIN =
            "SigninAndHistorySyncActivity.IsFullscreenSignin";
    private static final String ARGUMENT_CONFIG = "SigninAndHistorySyncActivity.Config";

    private static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    private final OneshotSupplierImpl<Profile> mProfileSupplier = new OneshotSupplierImpl<>();
    // TODO(crbug.com/349787455): Move this to FirstRunActivityBase.
    private final Promise<Void> mNativeInitializationPromise = new Promise<>();

    private boolean mIsFullscreenPromo;
    private SigninAndHistorySyncCoordinator mCoordinator;

    // Set to true when the add account activity is started, and is not persisted in saved instance
    // state. Therefore when onActivityResultWithNavitve is called with the add account activity's
    // result, if this boolean is false, it means that the activity is killed when the add account
    // activity was in the foreground.
    private boolean mIsWaitingForAddAccountResult;

    @Override
    protected void onPreCreate() {
        super.onPreCreate();
        // Temporarily ensure that the native is initialized before calling super.onCreate().
        // TODO(crbug.com/41493758): Handle the case where the UI is shown before the end of
        // native initialization.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
    }

    @Override
    public void triggerLayoutInflation() {
        super.triggerLayoutInflation();

        Intent intent = getIntent();
        int signinAccessPoint =
                intent.getIntExtra(ARGUMENT_ACCESS_POINT, SigninAccessPoint.MAX_VALUE);
        assert signinAccessPoint <= SigninAccessPoint.MAX_VALUE : "Cannot find SigninAccessPoint!";

        if (intent.getBooleanExtra(ARGUMENT_IS_FULLSCREEN_SIGNIN, false)) {
            updateSystemUiForFullscreenSignin();

            FullscreenSigninAndHistorySyncConfig config =
                    SigninAndHistorySyncBundleHelper.getFullscreenConfig(
                            intent.getBundleExtra(ARGUMENT_CONFIG));
            mIsFullscreenPromo = true;

            RecordHistogram.recordTimesHistogram(
                    "Signin.Timestamps.Android.Fullscreen.TriggerLayoutInflation",
                    SystemClock.elapsedRealtime() - getStartTime());

            mCoordinator =
                    new FullscreenSigninAndHistorySyncCoordinator(
                            getWindowAndroid(),
                            this,
                            getModalDialogManager(),
                            getProfileProviderSupplier(),
                            PrivacyPreferencesManagerImpl.getInstance(),
                            config,
                            signinAccessPoint,
                            this,
                            getStartTime(),
                            DeviceLockActivityLauncherImpl.get());

            setInitialContentView(mCoordinator.getView());
            onInitialLayoutInflationComplete();

            RecordHistogram.recordTimesHistogram(
                    "Signin.Timestamps.Android.Fullscreen.ActivityInflated",
                    SystemClock.elapsedRealtime() - getStartTime());
            return;
        }

        setStatusBarColor(Color.TRANSPARENT);

        BottomSheetSigninAndHistorySyncConfig config =
                SigninAndHistorySyncBundleHelper.getBottomSheetConfig(
                        intent.getBundleExtra(ARGUMENT_CONFIG));
        mCoordinator =
                new BottomSheetSigninAndHistorySyncCoordinator(
                        getWindowAndroid(),
                        this,
                        this,
                        DeviceLockActivityLauncherImpl.get(),
                        mProfileSupplier,
                        getModalDialogManagerSupplier(),
                        config,
                        signinAccessPoint);

        setInitialContentView(mCoordinator.getView());
        onInitialLayoutInflationComplete();
    }

    @Override
    protected void onPolicyLoadListenerAvailable(boolean onDevicePolicyFound) {
        super.onPolicyLoadListenerAvailable(onDevicePolicyFound);
        if (mIsFullscreenPromo) {
            RecordHistogram.recordTimesHistogram(
                    "Signin.Timestamps.Android.Fullscreen.PoliciesLoaded",
                    SystemClock.elapsedRealtime() - getStartTime());
        }
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
                    protected OtrProfileId createOffTheRecordProfileId() {
                        throw new IllegalArgumentException(
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
                this,
                /* listenToActivityState= */ true,
                getIntentRequestTracker(),
                getInsetObserver(),
                /* trackOcclusion= */ true);
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mNativeInitializationPromise.fulfill(null);
    }

    /**
     * Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate} and {@link
     * FullscreenSigninAndHistorySyncCoordinator.Delegate}.
     */
    @Override
    public void onFlowComplete(@SigninAndHistorySyncCoordinator.Result int result) {
        int resultCode =
                result == SigninAndHistorySyncCoordinator.Result.COMPLETED
                        ? Activity.RESULT_OK
                        : Activity.RESULT_CANCELED;
        setResult(resultCode);
        finish();
        // Override activity animation to avoid visual glitches due to the semi-transparent
        // background.
        overridePendingTransition(0, R.anim.fast_fade_out);
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public boolean isHistorySyncShownFullScreen() {
        return !isTablet();
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void setStatusBarColor(int statusBarColor) {
        StatusBarColorController.setStatusBarColor(
                (getEdgeToEdgeManager() != null)
                        ? getEdgeToEdgeManager().getEdgeToEdgeSystemBarColorHelper()
                        : null,
                getWindow(),
                statusBarColor);
    }

    @Override
    public void performOnConfigurationChanged(Configuration newConfig) {
        super.performOnConfigurationChanged(newConfig);
        mCoordinator.onConfigurationChange();
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent data) {
        if (super.onActivityResultWithNative(requestCode, resultCode, data)) {
            return true;
        }

        if (requestCode != ADD_ACCOUNT_REQUEST_CODE) {
            return false;
        }

        // If mIsWaitingForAddAccountResult is false, it means that the add account activity was not
        // started in this instance of Activity, and that the sign-in activity was killed during the
        // add account flow.
        if (!mIsWaitingForAddAccountResult) {
            SigninMetricsUtils.logAddAccountStateHistogram(State.ACTIVITY_DESTROYED);
        } else {
            SigninMetricsUtils.logAddAccountStateHistogram(State.ACTIVITY_SURVIVED);
        }

        mIsWaitingForAddAccountResult = false;
        onAddAccountResult(resultCode, data);
        return true;
    }

    @Override
    protected void onDestroy() {
        mCoordinator.destroy();
        super.onDestroy();
    }

    /** Implements {@link FirstRunActivityBase} */
    @Override
    public @BackPressResult int handleBackPress() {
        return mCoordinator.handleBackPress();
    }

    public static @NonNull Intent createIntent(
            @NonNull Context context,
            @NonNull BottomSheetSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint) {
        Intent intent = new Intent(context, SigninAndHistorySyncActivity.class);
        Bundle bundle = SigninAndHistorySyncBundleHelper.getBundle(config);
        intent.putExtra(ARGUMENT_CONFIG, bundle);
        intent.putExtra(ARGUMENT_ACCESS_POINT, signinAccessPoint);
        return intent;
    }

    public static @NonNull Intent createIntentForFullscreenSignin(
            Context context,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint) {
        Intent intent = new Intent(context, SigninAndHistorySyncActivity.class);
        intent.putExtra(ARGUMENT_IS_FULLSCREEN_SIGNIN, true);
        Bundle bundle = SigninAndHistorySyncBundleHelper.getBundle(config);
        intent.putExtra(ARGUMENT_CONFIG, bundle);
        intent.putExtra(ARGUMENT_ACCESS_POINT, signinAccessPoint);
        return intent;
    }

    /**
     * Implements {@link FullscreenSigninAndHistorySyncCoordinator.Delegate} and {@link
     * BottomSheetSigninAndHistorySyncCoordinator.Delegate}
     */
    @Override
    public void addAccount() {
        SigninMetricsUtils.logAddAccountStateHistogram(State.REQUESTED);
        AccountManagerFacadeProvider.getInstance()
                .createAddAccountIntent(
                        intent -> {
                            final ActivityWindowAndroid windowAndroid = getWindowAndroid();
                            if (windowAndroid == null) {
                                // The activity was shut down. Do nothing.
                                SigninMetricsUtils.logAddAccountStateHistogram(State.FAILED);
                                return;
                            }
                            if (intent == null) {
                                // AccountManagerFacade couldn't create the intent, use SigninUtils
                                // to open settings instead.
                                SigninMetricsUtils.logAddAccountStateHistogram(State.FAILED);
                                SigninUtils.openSettingsForAllAccounts(this);
                                return;
                            }
                            SigninMetricsUtils.logAddAccountStateHistogram(State.STARTED);
                            mIsWaitingForAddAccountResult = true;
                            startActivityForResult(intent, ADD_ACCOUNT_REQUEST_CODE);
                        });
    }

    /** Implements {@link FullscreenSigninAndHistorySyncCoordinator.Delegate} */
    @Override
    public Promise<Void> getNativeInitializationPromise() {
        return mNativeInitializationPromise;
    }

    private void setInitialContentView(View view) {
        assert view.getParent() == null;

        Intent intent = getIntent();
        if (intent.getBooleanExtra(ARGUMENT_IS_FULLSCREEN_SIGNIN, false)) {
            // Identically to the FRE, wrap the fullscreen sign-in flow UI inside a custom layout
            // which mimic DialogWhenLarge theme behavior.
            super.setContentView(SigninUtils.wrapInDialogWhenLargeLayout(view));
            return;
        }

        super.setContentView(view);
    }

    private void updateSystemUiForFullscreenSignin() {
        if (DialogWhenLargeContentLayout.shouldShowAsDialog(this)) {
            // Set status bar and navigation bar to dark if the promo is shown as a dialog.
            setStatusBarColor(Color.BLACK);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                // Use dark navigation bar.
                Window window = getWindow();
                window.setNavigationBarColor(Color.BLACK);
                window.setNavigationBarDividerColor(Color.BLACK);
                UiUtils.setNavigationBarIconColor(window.getDecorView().getRootView(), false);
            }
        } else {
            // Set the status bar color to the fullsceen sign-in background color.
            setStatusBarColor(SemanticColorUtils.getDefaultBgColor(this));
        }
    }

    private void onAddAccountResult(int resultCode, Intent data) {
        final String accountEmail =
                data == null
                        ? null
                        : IntentUtils.safeGetStringExtra(data, AccountManager.KEY_ACCOUNT_NAME);

        if (resultCode != Activity.RESULT_OK || accountEmail == null) {
            mCoordinator.onAddAccountCanceled();

            // Record NULL_ACCOUNT_NAME if the add account activity successfully returns but
            // contains a null account name.
            if (resultCode == Activity.RESULT_OK && accountEmail == null) {
                SigninMetricsUtils.logAddAccountStateHistogram(State.NULL_ACCOUNT_NAME);
            } else {
                SigninMetricsUtils.logAddAccountStateHistogram(State.CANCELLED);
            }
            return;
        }

        SigninMetricsUtils.logAddAccountStateHistogram(State.SUCCEEDED);
        mCoordinator.onAccountAdded(accountEmail);
    }
}
