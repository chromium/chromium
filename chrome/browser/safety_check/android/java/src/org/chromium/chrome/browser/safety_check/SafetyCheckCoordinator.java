// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.Observer;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Safety check settings page. */
public class SafetyCheckCoordinator implements DefaultLifecycleObserver {
    private SafetyCheckSettingsFragment mSettingsFragment;
    private SafetyCheckUpdatesDelegate mUpdatesClient;
    private SyncConsentActivityLauncher mSigninLauncher;
    private SafetyCheckMediator mMediator;

    /**
     * Creates a new instance given a settings fragment, an updates client, and a settings launcher.
     * There is no need to hold on to a reference since the settings fragment's lifecycle is
     * observed and a reference is retained there.
     *
     * @param settingsFragment An instance of {SafetyCheckSettingsFragment} to observe.
     * @param updatesClient An instance implementing the {@SafetyCheckUpdatesDelegate} interface.
     * @param settingsLauncher An instance implementing the {@SettingsLauncher} interface.
     * @param signinLauncher An instance implementing {@SigninActivityLauncher}.
     * @param modalDialogManagerSupplier An supplier for the {@ModalDialogManager}.
     */
    public static void create(SafetyCheckSettingsFragment settingsFragment,
            SafetyCheckUpdatesDelegate updatesClient, SettingsLauncher settingsLauncher,
            SyncConsentActivityLauncher signinLauncher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @Nullable SyncService syncService) {
        new SafetyCheckCoordinator(settingsFragment, updatesClient, settingsLauncher,
                signinLauncher, modalDialogManagerSupplier, syncService);
    }

    private SafetyCheckCoordinator(SafetyCheckSettingsFragment settingsFragment,
            SafetyCheckUpdatesDelegate updatesClient, SettingsLauncher settingsLauncher,
            SyncConsentActivityLauncher signinLauncher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @Nullable SyncService syncService) {
        mSettingsFragment = settingsFragment;
        mUpdatesClient = updatesClient;
        // Create the model and the mediator once the view is created.
        // The view's lifecycle is not available at this point, so observe the {@link LiveData} for
        // it to get notified when {@link onCreateView} is called.
        mSettingsFragment.getViewLifecycleOwnerLiveData().observe(
                mSettingsFragment, new Observer<LifecycleOwner>() {
                    @Override
                    public void onChanged(LifecycleOwner lifecycleOwner) {
                        // Only interested in the event when the View becomes non-null, which
                        // happens when {@link onCreateView} is invoked.
                        if (lifecycleOwner == null) {
                            return;
                        }
                        // Only initialize it if it hasn't been already. This guards against
                        // multiple invocations of this method.
                        if (mMediator == null) {
                            // Can start observing the View's lifecycle now.
                            lifecycleOwner.getLifecycle().addObserver(SafetyCheckCoordinator.this);
                            // The View is available, so now we can create the Model, MCP, and
                            // Mediator.
                            PropertyModel model = createModelAndMcp(mSettingsFragment);
                            mMediator = new SafetyCheckMediator(model, mUpdatesClient,
                                    settingsLauncher, signinLauncher, syncService,
                                    modalDialogManagerSupplier);
                        }
                    }
                });
        // Show the initial state every time the fragment is resumed (navigation from a different
        // screen, app in the background, etc).
        mSettingsFragment.getLifecycle().addObserver(new DefaultLifecycleObserver() {
            @Override
            public void onResume(LifecycleOwner lifecycleOwner) {
                if (mSettingsFragment.shouldRunSafetyCheckImmediately()) {
                    mMediator.performSafetyCheck();
                    return;
                }
                mMediator.setInitialState();
            }
        });
    }

    @VisibleForTesting
    static PropertyModel createModelAndMcp(SafetyCheckSettingsFragment settingsFragment) {
        PropertyModel model = SafetyCheckProperties.createSafetyCheckModel();
        PropertyModelChangeProcessor.create(model, settingsFragment, SafetyCheckViewBinder::bind);
        return model;
    }

    /** Gets invoked when the Fragment detaches (the View is destroyed ). */
    @Override
    public void onDestroy(LifecycleOwner owner) {
        // Stop observing the Lifecycle of the View as it is about to be destroyed.
        owner.getLifecycle().removeObserver(this);
        // Cancel any pending tasks.
        mMediator.destroy();
        // Clean up any objects we are holding on to.
        mSettingsFragment = null;
        mUpdatesClient = null;
        mMediator = null;
    }
}
