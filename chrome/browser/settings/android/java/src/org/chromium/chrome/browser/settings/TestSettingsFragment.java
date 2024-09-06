// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.settings.SettingsPage;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/** A TestSettingsFragment that has several preference inside. */
public class TestSettingsFragment extends PreferenceFragmentCompat
        implements SettingsPage, BackPressHandler {
    public static final String EXTRA_TITLE = "title";

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();

    private final CallbackHelper mBackPressCallback = new CallbackHelper();

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    public CallbackHelper getBackPressCallback() {
        return mBackPressCallback;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        addPreferencesFromResource(R.xml.test_settings_fragment);

        String title = "test title";
        Bundle args = getArguments();
        if (args != null) {
            String extraTitle = args.getString(EXTRA_TITLE);
            if (extraTitle != null) {
                title = extraTitle;
            }
        }
        mPageTitle.set(title);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
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
