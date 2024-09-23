// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
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

/** A class that provides functionality to block the initial draw for the Incognito restore flow. */
public class IncognitoRestoreAppLaunchDrawBlocker {
    /**
     * A key that is used to persist information about the last tab model selected to the saved
     * instance state.
     */
    public static final String IS_INCOGNITO_SELECTED = "is_incognito_selected";

    /** A {@link Supplier<Bundle>} for the saved instance state supplier. */
    private final @NonNull Supplier<Bundle> mSavedInstanceStateSupplier;

    /** A supplier of {@link TabModelSelector} instance.*/
    private final @NonNull ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    /**
     * A {@link ActivityLifecycleDispatcher} instance which allows to listen for {@link
     * NativeInitObserver} signals.
     */
    private final @NonNull ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * A {@link Supplier<Intent>} intent supplier which allows to get the intent if Chrome was
     * launched from one.
     */
    private final @NonNull Supplier<Intent> mIntentSupplier;

    /** A {@link Supplier<Boolean>} to indicate whether we should ignore the intent. */
    private final @NonNull Supplier<Boolean> mShouldIgnoreIntentSupplier;

    /**
     * A {@link Runnable} to unblock the draw operation. This is fired when both native and tab
     * state has been initialized.
     */
    private final @NonNull Runnable mUnblockDrawRunnable;

    private final @NonNull CipherFactory mCipherFactory;

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
    private CallbackController mCallbackController = new CallbackController();

    /**
     * A callback to add a {@link TabModelSelectorObserver} which notifies about the event when the
     * tab state is initialized. This is one of the signal along with the native initialization that
     * we look for to unblock the draw.
     */
    private final @NonNull Callback<TabModelSelector> mTabModelSelectorSupplierCallback =
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
     * @param tabModelSelectorSupplier A {@link ObservableSupplier<TabModelSelector>} that allows to
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
            @NonNull Supplier<Bundle> savedInstanceStateSupplier,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull Supplier<Intent> intentSupplier,
            @NonNull Supplier<Boolean> shouldIgnoreIntentSupplier,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Runnable unblockDrawRunnable,
            @NonNull CipherFactory cipherFactory) {
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mIntentSupplier = intentSupplier;
        mShouldIgnoreIntentSupplier = shouldIgnoreIntentSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mUnblockDrawRunnable = unblockDrawRunnable;
        mCipherFactory = cipherFactory;

        mActivityLifecycleDispatcher.register(mNativeInitObserver);
        mTabModelSelectorSupplier.addObserver(mTabModelSelectorSupplierCallback);
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

        // A valid saved instance state is needed here.
        if (!mSavedInstanceStateSupplier.hasValue()) return false;
        Bundle savedInstanceState = mSavedInstanceStateSupplier.get();

        if (!mCipherFactory.restoreFromBundle(savedInstanceState)) return false;

        // There were no Incognito tabs before the Activity got destroyed. So we don't need to block
        // draw here.
        if (!savedInstanceState.getBoolean(
                IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false)) {
            return false;
        }

        boolean isLastSelectedModelIncognito =
                savedInstanceState.getBoolean(IS_INCOGNITO_SELECTED, false);
        boolean isIncognitoFiredFromLauncherShortcut =
                !mShouldIgnoreIntentSupplier.get()
                        && mIntentSupplier.get() != null
                        && mIntentSupplier
                                .get()
                                .getBooleanExtra(
                                        IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB,
                                        false);

        // We have re-auth pending but we don't need to block draw if last tab model was regular and
        // we are not trying to create a new incognito tab from launcher shortcut.
        if (!isLastSelectedModelIncognito && !isIncognitoFiredFromLauncherShortcut) return false;

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

    private void maybeUnblockDraw() {
        if (!mTabModelSelectorSupplier.hasValue()) return;
        if (!mTabModelSelectorSupplier.get().isTabStateInitialized()) return;
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
