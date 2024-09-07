// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Tests for {@link PasswordAccessLossImportDialogCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordAccessLossImportDialogCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private PasswordAccessLossImportDialogCoordinator mCoordinator;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private FakeModalDialogManager mModalDialogManager;
    private Context mContext;
    @Mock private SyncService mSyncService;
    @Mock private PasswordManagerHelper mPasswordManagerHelper;
    @Mock private Runnable mChromeShutDownRunnable;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        mModalDialogManagerSupplier = () -> mModalDialogManager;
        mContext = RuntimeEnvironment.getApplication().getApplicationContext();
        mCoordinator =
                new PasswordAccessLossImportDialogCoordinator(
                        mContext,
                        mSyncService,
                        mModalDialogManagerSupplier,
                        mPasswordManagerHelper,
                        mChromeShutDownRunnable);
    }

    @Test
    public void testImportDialogStrings() {
        mCoordinator.showImportInstructionDialog();

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        Resources resources = RuntimeEnvironment.getApplication().getResources();
        assertEquals(
                resources.getString(
                        org.chromium.chrome.browser.password_manager.R.string
                                .access_loss_import_dialog_title),
                model.get(ModalDialogProperties.TITLE));
        assertEquals(
                resources.getString(
                        org.chromium.chrome.browser.password_manager.R.string
                                .access_loss_import_dialog_desc),
                model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
        assertEquals(
                resources.getString(
                        org.chromium.chrome.browser.password_manager.R.string
                                .access_loss_import_dialog_positive_button_text),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                resources.getString(org.chromium.chrome.browser.password_manager.R.string.cancel),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    public void testImportDialogOpensCredentialManagerAndShutsDownChrome() {
        mCoordinator.showImportInstructionDialog();
        Robolectric.flushForegroundThreadScheduler();

        mModalDialogManager.clickPositiveButton();
        verify(mPasswordManagerHelper)
                .launchTheCredentialManager(
                        eq(ManagePasswordsReferrer.ACCESS_LOSS_WARNING),
                        eq(mSyncService),
                        any(),
                        eq(mModalDialogManagerSupplier),
                        eq(mContext),
                        isNull());
        verify(mChromeShutDownRunnable).run();
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testCancelImportDialog() {
        mCoordinator.showImportInstructionDialog();
        Robolectric.flushForegroundThreadScheduler();

        mModalDialogManager.clickNegativeButton();
        verify(mPasswordManagerHelper, times(0))
                .launchTheCredentialManager(anyInt(), any(), any(), any(), any(), any());
        assertNull(mModalDialogManager.getShownDialogModel());
    }
}
