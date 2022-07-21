// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.os.Bundle;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
 * This is the access point for showing the Incognito re-auth dialog. It controls building the
 * {@link IncognitoReauthCoordinator} and showing/hiding the re-auth dialog. The {@link
 * IncognitoReauthCoordinator} is created and destroyed each time the dialog is shown and hidden.
 */
public class IncognitoReauthController
        implements IncognitoTabModelObserver.IncognitoReauthDialogDelegate,
                   StartStopWithNativeObserver, SaveInstanceStateObserver {
    // A key that would be persisted in saved instance that would be true if there were
    // incognito tabs present before Chrome went to background.
    public static final String KEY_IS_INCOGNITO_REAUTH_PENDING = "incognitoReauthPending";

    // This callback is fired when the user clicks on "Unlock Incognito" option.
    // This contains the logic to not require further re-authentication if the last one was a
    // success. Please note, a re-authentication would be required again when Chrome is brought to
    // foreground again.
    private final IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallback =
            new IncognitoReauthManager.IncognitoReauthCallback() {
                @Override
                public void onIncognitoReauthNotPossible() {
                    hideDialogIfShowing(DialogDismissalCause.ACTION_ON_DIALOG_NOT_POSSIBLE);
                }

                @Override
                public void onIncognitoReauthSuccess() {
                    mIncognitoReauthPending = false;
                    hideDialogIfShowing(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
                }

                @Override
                public void onIncognitoReauthFailure() {}
            };

    // If the user has closed all Incognito tabs, then they don't need to go through re-auth to open
    // fresh Incognito tabs.
    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void didBecomeEmpty() {
                    mIncognitoReauthPending = false;
                }
            };

    // An observer to handle cases for Incognito tabs restore cases.
    private final TabModelSelectorObserver mTabModelSelectorObserver =
            new TabModelSelectorObserver() {
                @Override
                public void onTabStateInitialized() {
                    onTabStateInitializedForReauth();
                }
            };

    // The {@link TabModelSelectorProfileSupplier} passed to the constructor may not have a {@link
    // Profile} set if the Tab state is not initialized yet. We make use of the {@link Profile} when
    // accessing {@link UserPrefs} in showDialogIfRequired. A null {@link Profile} would result in a
    // crash when accessing the pref. Therefore this callback is fired when the Profile is ready
    // which sets the |mProfile| and shows the re-auth dialog if required.
    private final Callback<Profile> mProfileSupplierCallback = new Callback<Profile>() {
        @Override
        public void onResult(@NonNull Profile profile) {
            mProfile = profile;
            showDialogIfRequired();
        }
    };

    // The {@link CallbackController} to populate the |mLayoutStateProvider| when it becomes
    // available.
    private final CallbackController mLayoutStateProviderCallbackController =
            new CallbackController();

    private final @NonNull ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final @NonNull TabModelSelector mTabModelSelector;
    private final @NonNull ObservableSupplier<Profile> mProfileObservableSupplier;
    private final @NonNull IncognitoReauthCoordinatorFactory mIncognitoReauthCoordinatorFactory;
    private final boolean mIsTabbedActivity;

    /**
     * {@link OnBackPressedCallback} which would be added to the fullscreen dialog, to handle
     * back-presses.
     */
    private final @NonNull OnBackPressedCallback mOnBackPressedInFullScreenReauthCallback =
            new OnBackPressedCallback(false) {
                @Override
                public void handleOnBackPressed() {
                    mBackPressInReauthFullScreenRunnable.run();
                }
            };
    /**
     * {@link Runnable} which would be called when back press is triggered when we are showing the
     * fullscreen re-auth. Back presses done from tab switcher re-auth screen, is handled elsewhere.
     */
    private final @NonNull Runnable mBackPressInReauthFullScreenRunnable;

    // No strong reference to this should be made outside of this class because
    // we set this to null in hideDialogIfShowing for it to be garbage collected.
    private @Nullable IncognitoReauthCoordinator mIncognitoReauthCoordinator;
    private @Nullable LayoutStateProvider mLayoutStateProvider;

    private @Nullable Profile mProfile;
    private boolean mIncognitoReauthPending;

    /**
     * @param tabModelSelector The {@link TabModelSelector} in order to interact with the
     *         regular/Incognito {@link TabModel}.
     * @param dispatcher The {@link ActivityLifecycleDispatcher} in order to register to
     *         onStartWithNative event.
     * @param layoutStateProviderOneshotSupplier A supplier of {@link LayoutStateProvider} which is
     *         used to determine the current {@link LayoutType} which is shown.
     * @param profileSupplier A Observable Supplier of {@link Profile} which is used to query the
     *         preference value of the Incognito lock setting.
     */
    public IncognitoReauthController(@NonNull TabModelSelector tabModelSelector,
            @NonNull ActivityLifecycleDispatcher dispatcher,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull IncognitoReauthCoordinatorFactory incognitoReauthCoordinatorFactory) {
        mTabModelSelector = tabModelSelector;
        mActivityLifecycleDispatcher = dispatcher;
        mProfileObservableSupplier = profileSupplier;
        mProfileObservableSupplier.addObserver(mProfileSupplierCallback);
        mIncognitoReauthCoordinatorFactory = incognitoReauthCoordinatorFactory;
        mIsTabbedActivity = mIncognitoReauthCoordinatorFactory.getIsTabbedActivity();
        mBackPressInReauthFullScreenRunnable =
                incognitoReauthCoordinatorFactory.getBackPressRunnable();

        layoutStateProviderOneshotSupplier.onAvailable(
                mLayoutStateProviderCallbackController.makeCancelable(layoutStateProvider -> {
                    mLayoutStateProvider = layoutStateProvider;
                    showDialogIfRequired();
                }));

        mTabModelSelector.setIncognitoReauthDialogDelegate(this);
        mTabModelSelector.addIncognitoTabModelObserver(mIncognitoTabModelObserver);
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mActivityLifecycleDispatcher.register(this);

        if (mTabModelSelector.isTabStateInitialized()) {
            // It may happen that the tab state was initialized before the
            // |mTabModelSelectorObserver| was added which explicitly takes care of restore case.
            // Therefore, we need another restore check here for such a case.
            onTabStateInitializedForReauth();
        }
    }

    /**
     * Should be called when the underlying {@link ChromeActivity} is destroyed.
     */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mTabModelSelector.setIncognitoReauthDialogDelegate(null);
        mTabModelSelector.removeIncognitoTabModelObserver(mIncognitoTabModelObserver);
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mProfileObservableSupplier.removeObserver(mProfileSupplierCallback);
        mLayoutStateProviderCallbackController.destroy();
        mIncognitoReauthCoordinatorFactory.destroy();
        mOnBackPressedInFullScreenReauthCallback.setEnabled(false);
        hideDialogIfShowing(DialogDismissalCause.ACTIVITY_DESTROYED);
    }

    /**
     * @return True, if the re-auth page is currently being shown, false otherwise.
     */
    public boolean isReauthPageShowing() {
        return mIncognitoReauthCoordinator != null;
    }

    /**
     * A method to check whether an Incognito re-authentication is currently pending.
     *
     * @return True, if an Incognito re-authentication is pending, false otherwise.
     */
    public boolean isIncognitoReauthPending() {
        // A re-authentication is pending only in the context when the re-auth setting is always on.
        return mIncognitoReauthPending && IncognitoReauthManager.isIncognitoReauthEnabled(mProfile);
    }

    /**
     * Override from {@link StartStopWithNativeObserver}. This relays the signal that Chrome was
     * brought to foreground.
     */
    @Override
    public void onStartWithNative() {
        showDialogIfRequired();
    }

    /**
     * Override from {@link SaveInstanceStateObserver}. This is called just before activity begins
     * to stop.
     */
    @Override
    public void onSaveInstanceState(Bundle outState) {
        mIncognitoReauthPending = (mTabModelSelector.getModel(/*incognito=*/true).getCount() > 0);
        if (mIncognitoReauthPending) {
            outState.putBoolean(KEY_IS_INCOGNITO_REAUTH_PENDING, mIncognitoReauthPending);
        }
    }

    /**
     * Override from {@link StartStopWithNativeObserver}.
     */
    @Override
    public void onStopWithNative() {
        // |mIncognitoReauthPending| also gets set in
        // IncognitoReauthController#onTabStateInitializedForReauth when the tab state is
        // initialized for the first time which is needed to handle cases of restored Incognito tabs
        // where a re-auth should be required. To tackle the more general case, we track the
        // onStopWithNative to update the |mIncognitoReauthPending| which gets called each time when
        // Chrome goes to background.
        mIncognitoReauthPending = (mTabModelSelector.getModel(/*incognito=*/true).getCount() > 0);
    }

    /**
     * Override from {@link IncognitoReauthDialogDelegate}.
     */
    @Override
    public void onAfterRegularTabModelChanged() {
        hideDialogIfShowing(DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);
    }

    /**
     * Override from {@link IncognitoReauthDialogDelegate}.
     */
    @Override
    public void onBeforeIncognitoTabModelSelected() {
        showDialogIfRequired();
    }

    /**
     * TODO(crbug.com/1227656): Add an extra check on IncognitoReauthManager#canAuthenticate method
     * if needed here to tackle the case where a re-authentication might not be possible from the
     * systems end in which case we should not show a re-auth dialog. The method currently doesn't
     * exists and may need to be exposed.
     */
    private void showDialogIfRequired() {
        if (mIncognitoReauthCoordinator != null) return;
        if (mLayoutStateProvider == null && mIsTabbedActivity) return;
        if (!mIncognitoReauthPending) return;
        if (!mTabModelSelector.isIncognitoSelected()) return;
        if (mProfile == null) return;
        if (!IncognitoReauthManager.isIncognitoReauthEnabled(mProfile)) return;

        boolean showFullScreen = !mIsTabbedActivity
                || !mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER);
        // TODO(crbug.com/1227656): Pass the |mOnBackPressedInFullScreenReauthCallback| dependency
        // to the coordinator.
        mIncognitoReauthCoordinator =
                mIncognitoReauthCoordinatorFactory.createIncognitoReauthCoordinator(
                        mIncognitoReauthCallback, showFullScreen);
        mIncognitoReauthCoordinator.show();
        mOnBackPressedInFullScreenReauthCallback.setEnabled(showFullScreen);
    }

    private void hideDialogIfShowing(@DialogDismissalCause int dismissalCause) {
        if (mIncognitoReauthCoordinator != null) {
            mOnBackPressedInFullScreenReauthCallback.setEnabled(false);
            mIncognitoReauthCoordinator.hide(dismissalCause);
            mIncognitoReauthCoordinator = null;
        }
    }

    // Tab state initialized is called when creating tabs from launcher shortcut or restore. Re-auth
    // dialogs should be shown iff any Incognito tabs were restored.
    private void onTabStateInitializedForReauth() {
        boolean hasRestoredIncognitoTabs = false;
        TabModel incognitoTabModel = mTabModelSelector.getModel(/*incognito=*/true);
        for (int i = 0; i < incognitoTabModel.getCount(); ++i) {
            @TabLaunchType
            Integer tabLaunchType = CriticalPersistedTabData.from(incognitoTabModel.getTabAt(i))
                                            .getTabLaunchTypeAtCreation();
            if (tabLaunchType == TabLaunchType.FROM_RESTORE) {
                hasRestoredIncognitoTabs = true;
                break;
            }
        }
        mIncognitoReauthPending = hasRestoredIncognitoTabs;
        showDialogIfRequired();
    }
}
