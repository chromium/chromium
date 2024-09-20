// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.StringRes;

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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Tests for {@link PasswordAccessLossWarningHelper} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordAccessLossWarningHelperTest {
    private PasswordAccessLossWarningHelper mHelper;
    private final ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);
    private Activity mActivity;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().start().resume().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        CustomTabIntentHelper customTabIntentHelper = (Context context, Intent intent) -> intent;
        mHelper =
                new PasswordAccessLossWarningHelper(
                        mActivity, mBottomSheetController, mProfile, customTabIntentHelper);
    }

    private void setUpBottomSheetController() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
    }

    private String getStringWithoutLink(@StringRes int stringId) {
        String sheetText = mActivity.getString(stringId);
        return SpanApplier.applySpans(
                        sheetText,
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(mActivity, view -> {})))
                .toString();
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
        assertEquals(
                model.get(SHEET_TITLE),
                mActivity.getString(R.string.pwd_access_loss_warning_no_gms_core_title));
        assertEquals(
                model.get(SHEET_TEXT).toString(),
                getStringWithoutLink(R.string.pwd_access_loss_warning_no_gms_core_text));
        assertEquals(
                model.get(BUTTON_TITLE),
                mActivity.getString(R.string.pwd_access_loss_warning_no_gms_core_button_text));
    }

    @Test
    public void getsModelForNoUpm() {
        PropertyModel model = mHelper.getModelForWarningType(PasswordAccessLossWarningType.NO_UPM);
        assertEquals(
                model.get(SHEET_TITLE),
                mActivity.getString(R.string.pwd_access_loss_warning_update_gms_core_title));
        assertEquals(
                model.get(SHEET_TEXT).toString(),
                getStringWithoutLink(R.string.pwd_access_loss_warning_update_gms_core_text));
        assertEquals(
                model.get(BUTTON_TITLE),
                mActivity.getString(R.string.pwd_access_loss_warning_update_gms_core_button_text));
    }

    @Test
    public void getsModelForOnlyAccountUpm() {
        PropertyModel model =
                mHelper.getModelForWarningType(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM);
        assertEquals(
                model.get(SHEET_TITLE),
                mActivity.getString(R.string.pwd_access_loss_warning_update_gms_core_title));
        assertEquals(
                model.get(SHEET_TEXT).toString(),
                getStringWithoutLink(R.string.pwd_access_loss_warning_update_gms_core_text));
        assertEquals(
                model.get(BUTTON_TITLE),
                mActivity.getString(R.string.pwd_access_loss_warning_update_gms_core_button_text));
    }

    @Test
    public void getsModelForFailedMigration() {
        PropertyModel model =
                mHelper.getModelForWarningType(
                        PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        assertEquals(
                model.get(SHEET_TITLE),
                mActivity.getString(R.string.pwd_access_loss_warning_manual_migration_title));
        assertEquals(
                model.get(SHEET_TEXT).toString(),
                mActivity
                        .getString(R.string.pwd_access_loss_warning_manual_migration_text)
                        .toString());
        assertEquals(
                model.get(BUTTON_TITLE),
                mActivity.getString(R.string.pwd_access_loss_warning_manual_migration_button_text));
    }
}
