// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.chromium.chrome.browser.incognito.reauth.IncognitoReauthControllerImpl.isFromUpdate;

import android.content.Intent;
import android.os.Bundle;
import android.os.PersistableBundle;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthControllerImpl;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.function.Supplier;

/** A class that provides functionality to block the initial draw for the Incognito restore flow. */
@NullMarked
public class IncognitoRestoreAppLaunchDrawBlocker {
    /**
     * A key that is used to persist information about the last tab model selected to the saved
     * instance state.
     */
    public static final String IS_INCOGNITO_SELECTED = "is_incognito_selected";

    public static final String SUPPORTED_PROFILE_TYPE = "supported_profile_type";

    /** A {@link Supplier<Bundle>} for the saved instance state supplier. */
    private final Supplier<Bundle> mSavedInstanceStateSupplier;

    /** A {@link Supplier<PersistableBundle>} for the persistent state supplier. */
    private final Supplier<PersistableBundle> mPersistentStateSupplier;

    /** A supplier of {@link TabModelSelector} instance. */
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    /**
     * A {@link ActivityLifecycleDispatcher} instance which allows to listen for {@link
     * NativeInitObserver} signals.
     */
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * A {@link Supplier<Intent>} intent supplier which allows to get the intent if Chrome was
     * launched from one.
     */
    private final Supplier<Intent> mIntentSupplier;

    /** A {@link Supplier<Boolean>} to indicate whether we should ignore the intent. */
    private final Supplier<Boolean> mShouldIgnoreIntentSupplier;

    /**
     * A {@link Runnable} to unblock the draw operation. This is fired when both native and tab
     * state has been initialized.
     */
    private final Runnable mUnblockDrawRunnable;

    private final CipherFactory mCipherFactory;

    /** A boolean so we don't fire unblock draw runnable twice. */
    private boolean mIsUnblockDrawRunnableInvoked;

    /**
     * An observer to listen for the onFinishNativeInitialization events. This signal works as a
     * fallback to unblock the draw,together with whether the tab state is initialized.
     */
    private final NativeInitObserver mNativeInitObserver =
            new NativeInitObserver() {
                @Override
                public void onFinishNativeInitialization() {
                    mIsNativeInitializationFinished = true;
                    maybeUnblockDraw();
                }
            };

    /**
     * The {@link CallbackController} for any callbacks that may run after the class is destroyed.
     */
    private final CallbackController mCallbackController = new CallbackController();

    /**
     * A callback to add a {@link TabModelSelectorObserver} which notifies about the event when the
     * tab state is initialized. This is one of the signal along with the native initialization that
     * we look for to unblock the draw.
     */
    private final Callback<TabModelSelector> mTabModelSelectorSupplierCallback =
            (tabModelSelector) -> {
                TabModelUtils.runOnTabStateInitialized(
                        tabModelSelector,
                        mCallbackController.makeCancelable(
                                unusedTabModelSelector -> maybeUnblockDraw()));
            };

    /**
     * A boolean to indicate when native has finished initialization as by then we would have
     * finished creating the {@link IncognitoReauthController}.
     */
    private boolean mIsNativeInitializationFinished;

    /**
     * @param savedInstanceStateSupplier A {@link Supplier<Bundle>} instance to pass in the bundle
     *     that was persisted during onSaveInstanceState that allows to look for signals on whether
     *     to block the draw or not.
     * @param persistentStateSupplier A {@link Supplier<PersistableBundle>} instance to pass in the
     *     PersistableBundle that was persisted during onSaveInstanceState that allows to look for
     *     signals on whether to block the draw or not.
     * @param tabModelSelectorSupplier A {@link MonotonicObservableSupplier <TabModelSelector>} that allows to
     *     listen for onTabStateInitialized signals which is used a fallback to unblock draw.
     * @param intentSupplier The {@link Supplier<Intent>} which is passed when Chrome was launched
     *     through Intent.
     * @param shouldIgnoreIntentSupplier A {@link Supplier<Boolean>} to indicate whether we need to
     *     ignore the intent.
     * @param activityLifecycleDispatcher A {@link ActivityLifecycleDispatcher} which would allow to
     *     listen for onFinishNativeInitialization signal.
     * @param unblockDrawRunnable A {@link Runnable} to unblock the draw operation.
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting.
     */
    IncognitoRestoreAppLaunchDrawBlocker(
            Supplier<Bundle> savedInstanceStateSupplier,
            Supplier<PersistableBundle> persistentStateSupplier,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<Intent> intentSupplier,
            Supplier<Boolean> shouldIgnoreIntentSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Runnable unblockDrawRunnable,
            CipherFactory cipherFactory) {
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
        mPersistentStateSupplier = persistentStateSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mIntentSupplier = intentSupplier;
        mShouldIgnoreIntentSupplier = shouldIgnoreIntentSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mUnblockDrawRunnable = unblockDrawRunnable;
        mCipherFactory = cipherFactory;

        mActivityLifecycleDispatcher.register(mNativeInitObserver);
        mTabModelSelectorSupplier.addSyncObserverAndPostIfNonNull(
                mTabModelSelectorSupplierCallback);
    }

    /** The destroy method which would remove any added observers by this class. */
    public void destroy() {
        mCallbackController.destroy();
        mActivityLifecycleDispatcher.unregister(mNativeInitObserver);
        mTabModelSelectorSupplier.removeObserver(mTabModelSelectorSupplierCallback);
    }

    /**
     * This method does some trivial level checks to quickly return false to unblock the draw. For
     * more complex case, it would unblock once "both" tab state and native is initialized.
     *
     * @return True, if we need to block the draw. False, otherwise.
     */
    public boolean shouldBlockDraw() {
        // We can't test for UserPrefs here since the native may not be initialized and we will
        // just wait for onTabStateInitialized to be triggered.
        if (!IncognitoReauthManager.isIncognitoReauthFeatureAvailable()) return false;
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.NO_RESTORE_STATE)) return false;

        boolean isIncognitoFiredFromLauncherShortcut =
                !mShouldIgnoreIntentSupplier.get()
                        && mIntentSupplier.get() != null
                        && mIntentSupplier
                                .get()
                                .getBooleanExtra(
                                        IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB,
                                        false);

        // A valid saved instance state or persistent state is needed here.
        boolean validSavedIncognitoState =
                hasValidSavedIncognitoState(isIncognitoFiredFromLauncherShortcut);
        boolean validPersistentIncognitoState =
                hasValidPersistentIncognitoState(isIncognitoFiredFromLauncherShortcut);
        if (!validSavedIncognitoState && !validPersistentIncognitoState) return false;

        // This is part of the environment that is required to make decisions. Therefore we need
        // this and we block.
        if (mTabModelSelectorSupplier.get() == null) return true;

        if (mTabModelSelectorSupplier.get().isTabStateInitialized()
                && mIsNativeInitializationFinished) {
            return false;
        }

        // We block the draw.
        return true;
    }

    private boolean hasValidSavedIncognitoState(boolean isIncognitoFiredFromLauncherShortcut) {
        Bundle savedInstanceState = mSavedInstanceStateSupplier.get();
        if (savedInstanceState == null) return false;

        if (!mCipherFactory.restoreFromBundle(savedInstanceState)) return false;

        // There were no Incognito tabs before the Activity got destroyed. So we don't need to block
        // draw here.
        if (!savedInstanceState.getBoolean(
                IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false)) {
            return false;
        }

        boolean isLastSelectedModelIncognito =
                savedInstanceState.getBoolean(IS_INCOGNITO_SELECTED, false);
        // We have re-auth pending but we don't need to block draw if last tab model was regular and
        // we are not trying to create a new incognito tab from launcher shortcut.
        if (!isLastSelectedModelIncognito && !isIncognitoFiredFromLauncherShortcut) return false;

        return true;
    }

    private boolean hasValidPersistentIncognitoState(boolean isIncognitoFiredFromLauncherShortcut) {
        PersistableBundle persistentState = mPersistentStateSupplier.get();
        if (persistentState == null) return false;

        // Only restore incognito state if the data was persisted for an app update.
        // TODO(crbug.com/474348773): Test more rigorously to see whether this check is needed.
        if (!isFromUpdate(persistentState)) {
            return false;
        }

        if (!mCipherFactory.restoreFromPersistableBundle(persistentState)) return false;

        // There were no Incognito tabs before the Activity got destroyed. So we don't need to block
        // draw here.
        if (!persistentState.getBoolean(
                IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false)) {
            return false;
        }

        boolean isLastSelectedModelIncognito =
                persistentState.getBoolean(IS_INCOGNITO_SELECTED, false);
        // We have re-auth pending but we don't need to block draw if last tab model was regular and
        // we are not trying to create a new incognito tab from launcher shortcut.
        if (!isLastSelectedModelIncognito && !isIncognitoFiredFromLauncherShortcut) return false;

        return true;
    }

    private void maybeUnblockDraw() {
        var tabModelSelector = mTabModelSelectorSupplier.get();
        if (tabModelSelector == null) return;
        if (!tabModelSelector.isTabStateInitialized()) return;
        if (!mIsNativeInitializationFinished) return;
        if (mIsUnblockDrawRunnableInvoked) return;

        mIsUnblockDrawRunnableInvoked = true;
        mUnblockDrawRunnable.run();
    }

    /** Test-only method. */
    public void resetIsUnblockDrawRunnableInvokedForTesting() {
        mIsUnblockDrawRunnableInvoked = false;
    }
}
