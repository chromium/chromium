// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.function.Supplier;

/** A factory class to create {@link IncognitoRestoreAppLaunchDrawBlocker}. */
@NullMarked
public class IncognitoRestoreAppLaunchDrawBlockerFactory {
    private final Supplier<Bundle> mSavedInstanceStateSupplier;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final CipherFactory mCipherFactory;

    /**
     * @param savedInstanceStateSupplier A {@link Supplier<Bundle>} instance to pass in the bundle
     *     that was persisted during onSaveInstanceState that allows to look for signals on whether
     *     to block the draw or not.
     * @param tabModelSelectorSupplier A {@link ObservableSupplier<TabModelSelector>} that allows to
     *     listen for onTabStateInitialized signals which is used a fallback to unblock draw.
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting.
     */
    public IncognitoRestoreAppLaunchDrawBlockerFactory(
            Supplier<Bundle> savedInstanceStateSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            CipherFactory cipherFactory) {
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mCipherFactory = cipherFactory;
    }

    /**
     * @param intentSupplier The {@link Supplier<Intent>} which is passed when Chrome was launched
     *     through Intent.
     * @param shouldIgnoreIntentSupplier A {@link Supplier<Boolean>} to indicate whether we need to
     *     ignore the intent.
     * @param activityLifecycleDispatcher A {@link ActivityLifecycleDispatcher} which would allow to
     *     listen for onFinishNativeInitialization signal.
     * @param unblockDrawRunnable A {@link Runnable} to unblock the draw operation.
     */
    IncognitoRestoreAppLaunchDrawBlocker create(
            Supplier<Intent> intentSupplier,
            Supplier<Boolean> shouldIgnoreIntentSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Runnable unblockDrawRunnable) {
        return new IncognitoRestoreAppLaunchDrawBlocker(
                mSavedInstanceStateSupplier,
                mTabModelSelectorSupplier,
                intentSupplier,
                shouldIgnoreIntentSupplier,
                activityLifecycleDispatcher,
                unblockDrawRunnable,
                mCipherFactory);
    }
}
