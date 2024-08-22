// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link PasswordAccessLossWarningHelper} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordAccessLossWarningHelperTest {
    private PasswordAccessLossWarningHelper mHelper;
    private final ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);
    private Context mContext;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Mock private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mContext = ContextUtils.getApplicationContext();

        mHelper = new PasswordAccessLossWarningHelper(mContext, mBottomSheetController);
    }

    private void setUpBottomSheetController() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
    }

    @Test
    public void showsSheet() {
        setUpBottomSheetController();
        mHelper.show(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mBottomSheetController).addObserver(any());
    }

    @Test
    public void getsModelForNoGmsCore() {
        PropertyModel model =
                mHelper.getModelForWarningType(PasswordAccessLossWarningType.NO_GMS_CORE);
        assertTrue(
                model.get(SHEET_TITLE)
                        .equals(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_no_gms_core_title)));
        assertTrue(
                model.get(SHEET_TEXT)
                        .equals(
                                mContext.getString(
                                                R.string.pwd_access_loss_warning_no_gms_core_text)
                                        .replace("<link>", "")
                                        .replace("</link>", "")));
        assertTrue(
                model.get(BUTTON_TITLE)
                        .equals(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_no_gms_core_button_text)));
    }

    @Test
    public void getsModelForNoUpm() {
        PropertyModel model = mHelper.getModelForWarningType(PasswordAccessLossWarningType.NO_UPM);
        assertTrue(
                model.get(SHEET_TITLE)
                        .equals(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_update_gms_core_title)));
        assertTrue(
                model.get(SHEET_TEXT)
                        .equals(
                                mContext.getString(
                                                R.string
                                                        .pwd_access_loss_warning_update_gms_core_text)
                                        .replace("<link>", "")
                                        .replace("</link>", "")));
        assertTrue(
                model.get(BUTTON_TITLE)
                        .equals(
                                mContext.getString(
                                        R.string
                                                .pwd_access_loss_warning_update_gms_core_button_text)));
    }

    @Test
    public void getsModelForOnlyAccountUpm() {
        PropertyModel model =
                mHelper.getModelForWarningType(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM);
        assertTrue(
                model.get(SHEET_TITLE)
                        .equals(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_update_gms_core_title)));
        assertTrue(
                model.get(SHEET_TEXT)
                        .equals(
                                mContext.getString(
                                                R.string
                                                        .pwd_access_loss_warning_update_gms_core_text)
                                        .replace("<link>", "")
                                        .replace("</link>", "")));
        assertTrue(
                model.get(BUTTON_TITLE)
                        .equals(
                                mContext.getString(
                                        R.string
                                                .pwd_access_loss_warning_update_gms_core_button_text)));
    }

    @Test
    public void getsModelForFailedMigration() {
        PropertyModel model =
                mHelper.getModelForWarningType(
                        PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        assertTrue(
                model.get(SHEET_TITLE)
                        .equals(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_manual_migration_title)));
        assertTrue(
                model.get(SHEET_TEXT)
                        .equals(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_manual_migration_text)));
        assertTrue(
                model.get(BUTTON_TITLE)
                        .equals(
                                mContext.getString(
                                        R.string
                                                .pwd_access_loss_warning_manual_migration_button_text)));
    }
}
