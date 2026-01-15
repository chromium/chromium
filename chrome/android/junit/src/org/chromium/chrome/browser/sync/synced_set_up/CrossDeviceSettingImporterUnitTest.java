// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;

import java.util.function.Supplier;

/** Unit tests for {@link CrossDeviceSettingImporter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CrossDeviceSettingImporterUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private Supplier<SnackbarManager> mSnackbarManagerSupplier;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Snackbar mSnackbar;
    @Mock private NullableObservableSupplier<Tab> mActivityTabSupplier;

    @Captor private ArgumentCaptor<ModalDialogManagerObserver> mModalDialogManagerObserverCaptor;

    private CrossDeviceSettingImporter mCrossDeviceSettingImporter;

    @Before
    public void setUp() {
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        when(mSnackbarManagerSupplier.get()).thenReturn(mSnackbarManager);
        mCrossDeviceSettingImporter =
                new CrossDeviceSettingImporter(
                        mActivityLifecycleDispatcher,
                        mActivityTabSupplier,
                        mModalDialogManagerSupplier,
                        mSnackbarManagerSupplier);
    }

    @Test
    public void testShowSnackbarAfterDialogs_noDialogs() {
        when(mModalDialogManager.isShowing()).thenReturn(false);
        mCrossDeviceSettingImporter.showSnackbarAfterDialogs(mSnackbar);
        verify(mSnackbarManager).showSnackbar(mSnackbar);
    }

    @Test
    public void testShowSnackbarAfterDialogs_withDialog() {
        when(mModalDialogManager.isShowing()).thenReturn(true);
        mCrossDeviceSettingImporter.showSnackbarAfterDialogs(mSnackbar);
        verify(mModalDialogManager).addObserver(mModalDialogManagerObserverCaptor.capture());

        // Simulate dialog dismissal.
        mModalDialogManagerObserverCaptor.getValue().onLastDialogDismissed();
        verify(mSnackbarManager).showSnackbar(mSnackbar);
    }
}
