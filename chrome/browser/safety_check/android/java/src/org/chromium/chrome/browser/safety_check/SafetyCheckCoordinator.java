// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Handler;

import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.Observer;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckControllerFactory;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Safety check settings page. */
@NullMarked
public class SafetyCheckCoordinator implements DefaultLifecycleObserver, SafetyCheckComponentUi {
    private SafetyCheckSettingsFragment mSettingsFragment;
    private SafetyCheckUpdatesDelegate mUpdatesClient;
    private @MonotonicNonNull SafetyCheckMediator mMediator;
    private final @Nullable SyncService mSyncService;
    private @Nullable PasswordStoreBridge mPasswordStoreBridge;
    private @Nullable PropertyModel mPasswordCheckLocalModel;
    private @Nullable PropertyModel mPasswordCheckAccountModel;

    /**
     * Creates a new instance given a settings fragment, an updates client, and a settings launcher.
     * There is no need to hold on to a reference since the settings fragment's lifecycle is
     * observed and a reference is retained there.
     *
     * @param settingsFragment An instance of {@link SafetyCheckSettingsFragment} to observe.
     * @param updatesClient An instance implementing the {@link SafetyCheckUpdatesDelegate}
     *     interface.
     * @param bridge An instances of {@link SafetyCheckBridge} to access C++ APIs.
     * @param modalDialogManagerSupplier An supplier for the {@link ModalDialogManager}.
     * @param passwordStoreBridge Provides access to stored passwords.
     * @param passwordManagerHelper An instance of {@link PasswordManagerHelper} that provides
     *     access to password management capabilities.
     * @param settingsCustomTabLauncher Used by password manager to open a help center article in a
     *     custom tab.
     */
    public static void create(
            SafetyCheckSettingsFragment settingsFragment,
            SafetyCheckUpdatesDelegate updatesClient,
            SafetyCheckBridge bridge,
            ObservableSupplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            @Nullable SyncService syncService,
            PasswordStoreBridge passwordStoreBridge,
            PasswordManagerHelper passwordManagerHelper,
            SettingsCustomTabLauncher settingsCustomTabLauncher) {
        new SafetyCheckCoordinator(
                settingsFragment,
                updatesClient,
                bridge,
                modalDialogManagerSupplier,
                syncService,
                passwordStoreBridge,
                passwordManagerHelper,
                settingsCustomTabLauncher);
    }

    private SafetyCheckCoordinator(
            SafetyCheckSettingsFragment settingsFragment,
            SafetyCheckUpdatesDelegate updatesClient,
            SafetyCheckBridge bridge,
            ObservableSupplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            @Nullable SyncService syncService,
            PasswordStoreBridge passwordStoreBridge,
            PasswordManagerHelper passwordManagerHelper,
            SettingsCustomTabLauncher settingsCustomTabLauncher) {
        mSettingsFragment = settingsFragment;
        mUpdatesClient = updatesClient;
        mSyncService = syncService;
        mPasswordStoreBridge = passwordStoreBridge;
        mSettingsFragment.setComponentDelegate(this);
        // Create the model and the mediator once the view is created.
        // The view's lifecycle is not available at this point, so observe the {@link LiveData} for
        // it to get notified when {@link onCreateView} is called.
        mSettingsFragment
                .getViewLifecycleOwnerLiveData()
                .observe(
                        mSettingsFragment,
                        new Observer<>() {
                            @Override
                            public void onChanged(LifecycleOwner lifecycleOwner) {
                                // Only interested in the event when the View becomes non-null,
                                // which happens when {@link onCreateView} is invoked.
                                if (lifecycleOwner == null) {
                                    return;
                                }
                                // Only initialize it if it hasn't been already. This guards against
                                // multiple invocations of this method.
                                if (mMediator == null) {
                                    // Can start observing the View's lifecycle now.
                                    lifecycleOwner
                                            .getLifecycle()
                                            .addObserver(SafetyCheckCoordinator.this);
                                    // The View is available, so now we can create the Model, MCP,
                                    // and Mediator.
                                    PropertyModel safetyCheckModel =
                                            createSafetyCheckModelAndBind(mSettingsFragment);
                                    createPasswordCheckModels(mSettingsFragment, safetyCheckModel);
                                    mMediator =
                                            new SafetyCheckMediator(
                                                    safetyCheckModel,
                                                    mPasswordCheckAccountModel,
                                                    mPasswordCheckLocalModel,
                                                    mUpdatesClient,
                                                    bridge,
                                                    syncService,
                                                    new Handler(),
                                                    passwordStoreBridge,
                                                    new PasswordCheckControllerFactory(),
                                                    passwordManagerHelper,
                                                    modalDialogManagerSupplier,
                                                    settingsCustomTabLauncher);
                                }
                            }
                        });
        // Show the initial state every time the fragment is resumed (navigation from a different
        // screen, app in the background, etc).
        mSettingsFragment
                .getLifecycle()
                .addObserver(
                        new DefaultLifecycleObserver() {
                            @Override
                            public void onResume(LifecycleOwner lifecycleOwner) {
                                assumeNonNull(mMediator);
                                if (mSettingsFragment.shouldRunSafetyCheckImmediately()) {
                                    mMediator.performSafetyCheck();
                                    return;
                                }
                                mMediator.setInitialState();
                            }
                        });
        // Clean up any objects we are holding on to when the fragment is destroyed.
        mSettingsFragment
                .getLifecycle()
                .addObserver(
                        new DefaultLifecycleObserver() {
                            @Override
                            @SuppressWarnings("NullAway")
                            public void onDestroy(LifecycleOwner lifecycleOwner) {
                                mSettingsFragment = null;
                                mUpdatesClient = null;
                                mPasswordStoreBridge.destroy();
                                mPasswordStoreBridge = null;
                            }
                        });
    }

    @VisibleForTesting
    static PropertyModel createSafetyCheckModelAndBind(
            SafetyCheckSettingsFragment settingsFragment) {
        PropertyModel model = SafetyCheckProperties.createSafetyCheckModel();
        PropertyModelChangeProcessor.create(model, settingsFragment, SafetyCheckViewBinder::bind);
        return model;
    }

    private void createPasswordCheckModels(
            SafetyCheckSettingsFragment settingsFragment, PropertyModel safetyCheckModel) {
        if (isAccountPasswordStorageUsed()) {
            String title =
                    mSettingsFragment.getString(
                            R.string.safety_check_passwords_account_title,
                            CoreAccountInfo.getEmailFrom(mSyncService.getAccountInfo()));
            mPasswordCheckAccountModel =
                    createPasswordCheckPreferenceModelAndBind(
                            settingsFragment,
                            safetyCheckModel,
                            SafetyCheckViewBinder.PASSWORDS_KEY_ACCOUNT,
                            title);
        }
        String title = mSettingsFragment.getString(R.string.safety_check_passwords_local_title);
        mPasswordCheckLocalModel =
                createPasswordCheckPreferenceModelAndBind(
                        settingsFragment,
                        safetyCheckModel,
                        SafetyCheckViewBinder.PASSWORDS_KEY_LOCAL,
                        title);
    }

    static PropertyModel createPasswordCheckPreferenceModelAndBind(
            SafetyCheckSettingsFragment settingsFragment,
            PropertyModel safetyCheckModel,
            String preferenceViewId,
            String preferenceTitle) {
        PropertyModel passwordSafetyCheckModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel(preferenceTitle);
        PropertyModelChangeProcessor.create(
                passwordSafetyCheckModel,
                settingsFragment,
                (model, fragment, key) ->
                        SafetyCheckViewBinder.bindPasswordCheckPreferenceModel(
                                safetyCheckModel, model, fragment, key, preferenceViewId));
        return passwordSafetyCheckModel;
    }

    /**
     * Gets invoked when the Fragment's view is destroyed.
     *
     * <p>This method can be called back for several different views as a fragment may create
     * multiple views (e.g. when the user navigation pushes the fragment into the back stack and
     * then pops it).
     */
    @SuppressWarnings("NullAway")
    @Override
    public void onDestroy(LifecycleOwner owner) {
        // Stop observing the Lifecycle of the View as it is about to be destroyed.
        owner.getLifecycle().removeObserver(this);
        // Cancel any pending tasks.
        mMediator.destroy();
        mMediator = null;
    }

    @Override
    @EnsuresNonNullIf("mSyncService")
    public boolean isAccountPasswordStorageUsed() {
        return mSyncService != null && PasswordManagerHelper.hasChosenToSyncPasswords(mSyncService);
    }
}
