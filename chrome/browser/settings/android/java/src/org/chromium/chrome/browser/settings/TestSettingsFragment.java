// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/** A TestSettingsFragment that has several preference inside. */
public class TestSettingsFragment extends PreferenceFragmentCompat implements BackPressHandler {
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();

    private final CallbackHelper mBackPressCallback = new CallbackHelper();

    public CallbackHelper getBackPressCallback() {
        return mBackPressCallback;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        addPreferencesFromResource(R.xml.test_settings_fragment);
    }

    @Override
    public int handleBackPress() {
        mBackPressCallback.notifyCalled();
        return BackPressResult.SUCCESS;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }
}
