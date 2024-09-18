// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/** A standalone settings fragment that has several preference inside. */
public class TestStandaloneFragment extends PreferenceFragmentCompat implements BackPressHandler {
    public static final String EXTRA_TITLE = "title";

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();

    private final CallbackHelper mBackPressCallback = new CallbackHelper();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        addPreferencesFromResource(R.xml.test_settings_fragment);

        String title = "standalone";
        Bundle args = getArguments();
        if (args != null) {
            String extraTitle = args.getString(EXTRA_TITLE);
            if (extraTitle != null) {
                title = extraTitle;
            }
        }
        requireActivity().setTitle(title);
    }

    public CallbackHelper getBackPressCallback() {
        return mBackPressCallback;
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
