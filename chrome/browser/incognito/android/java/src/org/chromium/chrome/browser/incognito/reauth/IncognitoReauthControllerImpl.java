// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.os.Bundle;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;

import java.util.ArrayList;
import java.util.List;

/**
 * This is the access point for showing the Incognito re-auth dialog. It controls building the
 * {@link IncognitoReauthCoordinator} and showing/hiding the re-auth dialog. The {@link
 * IncognitoReauthCoordinator} is created and destroyed each time the dialog is shown and hidden.
 *
 * <p>TODO(crbug.com/40056462): Change the scope of this to make it package protected and design a
 * way to create and destroy this for {@link RootUiCoordinator}.
 */
public class IncognitoReauthControllerImpl
        implements IncognitoReauthController,
                IncognitoTabModelObserver.IncognitoReauthDialogDelegate,
                StartStopWithNativeObserver,
                SaveInstanceStateObserver,
                ApplicationStatus.TaskVisibilityListener {
    // A key that would be persisted in saved instance that would be true if there were
    // incognito tabs present before Chrome went to background.
    public static final String KEY_IS_INCOGNITO_REAUTH_PENDING = "incognitoReauthPending";

    /**
     * A list of all {@link IncognitoReauthCallback} that would be triggered from
     * |mIncognitoReauthCallback|.
     */
    private final List<IncognitoReauthManager.IncognitoReauthCallback>
            mIncognitoReauthCallbackList = new ArrayList<>();

    // This callback is fired when the user clicks on "Unlock Incognito" option.
    // This contains the logic to not require further re-authentication if the last one was a
    // success. Please note, a re-authentication would be required again when Chrome is brought to
    // foreground again.
    private final IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallback =
            new IncognitoReauthManager.IncognitoReauthCallback() {
                @Override
                public void onIncognitoReauthNotPossible() {
                    hideDialogIfShowing(DialogDismissalCause.ACTION_ON_DIALOG_NOT_POSSIBLE);
                    for (IncognitoReauthManager.IncognitoReauthCallback callback :
                            mIncognitoReauthCallbackList) {
                        callback.onIncognitoReauthNotPossible();
                    }
                }

                @Override
                public void onIncognitoReauthSuccess() {
                    mIncognitoReauthPending = false;
                    hideDialogIfShowing(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
                    for (IncognitoReauthManager.IncognitoReauthCallback callback :
                            mIncognitoReauthCallbackList) {
                        callback.onIncognitoReauthSuccess();
                    }
                }

                @Override
                public void onIncognitoReauthFailure() {
                    for (IncognitoReauthManager.IncognitoReauthCallback callback :
                            mIncognitoReauthCallbackList) {
                        callback.onIncognitoReauthFailure();
                    }
                }
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

    private final LayoutStateProvider.LayoutStateObserver mLayoutStateObserver =
            new LayoutStateProvider.LayoutStateObserver() {
                @Override
                public void onStartedShowing(int layoutType) {
                    if (layoutType == LayoutType.BROWSING) {
                        showDialogIfRequired();
                    }
                }

                @Override
                public void onFinishedHiding(int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        hideDialogIfShowing(DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);
                    }
                }
            };

    // The {@link TabModelSelectorProfileSupplier} passed to the constructor may not have a {@link
    // Profile} set if the Tab state is not initialized yet. We make use of the {@link Profile} when
    // accessing {@link UserPrefs} in showDialogIfRequired. A null {@link Profile} would result in a
    // crash when accessing the pref. Therefore this callback is fired when the Profile is ready
    // which sets the |mProfile| and shows the re-auth dialog if required.
    private final Callback<Profile> mProfileSupplierCallback =
            new Callback<Profile>() {
                @Override
                public void onResult(@NonNull Profile profile) {
                    mProfile = profile;
                    showDialogIfRequired();
                    if (!mIsStartupMetricsRecorded) {
                        RecordHistogram.recordBooleanHistogram(
                                "Android.IncognitoReauth.ToggleOnOrOff",
                                IncognitoReauthManager.isIncognitoReauthEnabled(mProfile));
                        mIsStartupMetricsRecorded = true;
                    }
                }
            };

    /**
     * The {@link CallbackController} for any callbacks that may run after the class is destroyed.
     */
    private final CallbackController mCallbackController = new CallbackController();

    private final @NonNull ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final @NonNull TabModelSelector mTabModelSelector;
    private final @NonNull ObservableSupplier<Profile> mProfileObservableSupplier;
    private final @NonNull IncognitoReauthCoordinatorFactory mIncognitoReauthCoordinatorFactory;
    private final int mTaskId;
    private final boolean mIsTabbedActivity;

    /**
     * {@link OnBackPressedCallback} which would be added to the "fullscreen" dialog, to handle
     * back-presses. The back press handling for re-auth view shown inside tab switcher is
     * controlled elsewhere.
     *
     * Note that {@link BackPressManager} doesn't support handling back presses done
     * when a dialog is being shown. The integration of back press for app modal dialog is done
     * via {@link ModalDialogProperties#APP_MODAL_DIALOG_BACK_PRESS_HANDLER}.
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

    /**
     * A supplier to indicate if the re-auth was pending in the previous Chrome session before it
     * was destroyed.
     */
    private final @NonNull Supplier<Boolean> mIsIncognitoReauthPendingOnRestoreSupplier;

    // No strong reference to this should be made outside of this class because
    // we set this to null in hideDialogIfShowing for it to be garbage collected.
    private @Nullable IncognitoReauthCoordinator mIncognitoReauthCoordinator;
    private @Nullable LayoutStateProvider mLayoutStateProvider;

    private @Nullable Profile mProfile;
    private boolean mIsStartupMetricsRecorded;
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
     * @param incognitoReauthPendingOnRestoreSupplier Supplier to indicate where the {@link
     *         IncognitoReauthControllerImpl#KEY_IS_INCOGNITO_REAUTH_PENDING} was set to true in the
     * saved instance state.
     * @param taskId The task Id of the {@link ChromeActivity} associated with this controller.
     */
    public IncognitoReauthControllerImpl(
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ActivityLifecycleDispatcher dispatcher,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull IncognitoReauthCoordinatorFactory incognitoReauthCoordinatorFactory,
            @NonNull Supplier<Boolean> incognitoReauthPendingOnRestoreSupplier,
            int taskId) {
        mTabModelSelector = tabModelSelector;
        mActivityLifecycleDispatcher = dispatcher;
        mProfileObservableSupplier = profileSupplier;
        mProfileObservableSupplier.addObserver(mProfileSupplierCallback);
        mIncognitoReauthCoordinatorFactory = incognitoReauthCoordinatorFactory;
        mIsTabbedActivity = mIncognitoReauthCoordinatorFactory.getIsTabbedActivity();
        mBackPressInReauthFullScreenRunnable =
                incognitoReauthCoordinatorFactory.getBackPressRunnable();
        mTaskId = taskId;
        mIsIncognitoReauthPendingOnRestoreSupplier = incognitoReauthPendingOnRestoreSupplier;

        layoutStateProviderOneshotSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        layoutStateProvider -> {
                            mLayoutStateProvider = layoutStateProvider;
                            mLayoutStateProvider.addObserver(mLayoutStateObserver);
                            showDialogIfRequired();
                        }));
        incognitoReauthCoordinatorFactory
                .getTabSwitcherCustomViewManagerSupplier()
                .onAvailable(
                        mCallbackController.makeCancelable(
                                ignored -> {
                                    showDialogIfRequired();
                                }));

        mTabModelSelector.setIncognitoReauthDialogDelegate(this);
        mTabModelSelector.addIncognitoTabModelObserver(mIncognitoTabModelObserver);

        mActivityLifecycleDispatcher.register(this);
        ApplicationStatus.registerTaskVisibilityListener(this);

        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        unusedTabModelSelector -> onTabStateInitializedForReauth()));
    }

    /**
     * Override from {@link IncognitoReauthController}.
     *
     * Should be called when the underlying {@link ChromeActivity} is destroyed.
     */
    @Override
    public void destroy() {
        ApplicationStatus.unregisterTaskVisibilityListener(this);
        mActivityLifecycleDispatcher.unregister(this);
        mTabModelSelector.setIncognitoReauthDialogDelegate(null);
        mTabModelSelector.removeIncognitoTabModelObserver(mIncognitoTabModelObserver);
        mProfileObservableSupplier.removeObserver(mProfileSupplierCallback);
        mCallbackController.destroy();
        mIncognitoReauthCoordinatorFactory.destroy();
        mOnBackPressedInFullScreenReauthCallback.setEnabled(false);

        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }

        if (mIncognitoReauthCoordinator != null) {
            mIncognitoReauthCoordinator.destroy();
            mIncognitoReauthCoordinator = null;
        }
    }

    /** Override from {@link IncognitoReauthController}. */
    @Override
    public boolean isReauthPageShowing() {
        return mIncognitoReauthCoordinator != null;
    }

    /** Override from {@link IncognitoReauthController}. */
    @Override
    public boolean isIncognitoReauthPending() {
        // A re-authentication is pending only in the context when the re-auth setting is always on.
        return mIncognitoReauthPending && IncognitoReauthManager.isIncognitoReauthEnabled(mProfile);
    }

    /** Override from {@link IncognitoReauthController}. */
    @Override
    public void addIncognitoReauthCallback(
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback) {
        if (!mIncognitoReauthCallbackList.contains(incognitoReauthCallback)) {
            mIncognitoReauthCallbackList.add(incognitoReauthCallback);
        }
    }

    /** Override from {@link IncognitoReauthController}. */
    @Override
    public void removeIncognitoReauthCallback(
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback) {
        mIncognitoReauthCallbackList.remove(incognitoReauthCallback);
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
        // TODO(crbug.com/40242374): Incognito does not lock correctly for versions < Android P.
        outState.putBoolean(KEY_IS_INCOGNITO_REAUTH_PENDING, mIncognitoReauthPending);
    }

    /** Override from {@link StartStopWithNativeObserver}. */
    @Override
    public void onStopWithNative() {}

    /** Override from {@link IncognitoReauthDialogDelegate}. */
    @Override
    public void onAfterRegularTabModelChanged() {
        hideDialogIfShowing(DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);
    }

    /** Override from {@link IncognitoReauthDialogDelegate}. */
    @Override
    public void onBeforeIncognitoTabModelSelected() {
        showDialogIfRequired();
    }

    /** Override from {@link TaskVisibility.TaskVisibilityObserver} */
    @Override
    public void onTaskVisibilityChanged(int taskId, boolean isVisible) {
        if (taskId != mTaskId) return;
        if (!isVisible) {
            mIncognitoReauthPending =
                    mTabModelSelector.getModel(/* incognito= */ true).getCount() > 0;
        }
    }

    IncognitoReauthManager.IncognitoReauthCallback getIncognitoReauthCallbackForTesting() {
        return mIncognitoReauthCallback;
    }

    /**
     * TODO(crbug.com/40056462): Add an extra check on IncognitoReauthManager#canAuthenticate method
     * if needed here to tackle the case where a re-authentication might not be possible from the
     * systems end in which case we should not show a re-auth dialog. The method currently doesn't
     * exists and may need to be exposed.
     */
    private void showDialogIfRequired() {
        if (mIncognitoReauthCoordinator != null) return;
        if (mLayoutStateProvider == null && mIsTabbedActivity) return;
        if (!mIncognitoReauthPending) return;
        if (!mTabModelSelector.isIncognitoBrandedModelSelected()) return;
        if (mProfile == null) return;
        if (!IncognitoReauthManager.isIncognitoReauthEnabled(mProfile)) return;

        boolean showFullScreen =
                !mIsTabbedActivity
                        || !mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER);
        if (!mIncognitoReauthCoordinatorFactory.areDependenciesReadyFor(showFullScreen)) {
            return;
        }
        mIncognitoReauthCoordinator =
                mIncognitoReauthCoordinatorFactory.createIncognitoReauthCoordinator(
                        mIncognitoReauthCallback,
                        showFullScreen,
                        mOnBackPressedInFullScreenReauthCallback);
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
        mIncognitoReauthPending =
                mTabModelSelector.getModel(/* incognito= */ true).getCount() > 0
                        && mIsIncognitoReauthPendingOnRestoreSupplier.get();
        showDialogIfRequired();
    }
}
