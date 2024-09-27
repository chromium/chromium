// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.DISMISS;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.HELP_CENTER;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.MAIN_ACTION;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.getUserActionHistogramName;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_ACTION;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason.BACK_PRESS;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.text.SpannableString;
import android.view.View;

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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.List;

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

    private void clickSpan(SpannableString spannableString) {
        NoUnderlineClickableSpan[] spans =
                spannableString.getSpans(
                        0, spannableString.length(), NoUnderlineClickableSpan.class);
        spans[0].onClick(new View(mActivity.getBaseContext()));
    }

    private void notifyBottomSheetObservers(
            List<BottomSheetObserver> observers, @StateChangeReason int reason) {
        assertNotNull(observers);

        for (BottomSheetObserver observer : observers) {
            observer.onSheetClosed(reason);
        }
    }

    @Test
    public void showsSheet() {
        setUpBottomSheetController();
        mHelper.show(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mBottomSheetController, times(2)).addObserver(any());
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

    @Test
    public void mainActionForNoGmsCore() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(
                                        PasswordAccessLossWarningType.NO_GMS_CORE),
                                MAIN_ACTION)
                        .build();

        PropertyModel model =
                mHelper.getModelForWarningType(PasswordAccessLossWarningType.NO_GMS_CORE);
        model.get(BUTTON_ACTION).run();

        histogram.assertExpected();
    }

    @Test
    public void mainActionForNoUpm() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(PasswordAccessLossWarningType.NO_UPM),
                                MAIN_ACTION)
                        .build();

        PropertyModel model = mHelper.getModelForWarningType(PasswordAccessLossWarningType.NO_UPM);
        model.get(BUTTON_ACTION).run();

        histogram.assertExpected();
    }

    @Test
    public void mainActionForOnlyAccountUpm() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(
                                        PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM),
                                MAIN_ACTION)
                        .build();

        PropertyModel model =
                mHelper.getModelForWarningType(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM);
        model.get(BUTTON_ACTION).run();

        histogram.assertExpected();
    }

    @Test
    public void mainActionForFailedMigration() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(
                                        PasswordAccessLossWarningType
                                                .NEW_GMS_CORE_MIGRATION_FAILED),
                                MAIN_ACTION)
                        .build();

        PropertyModel model =
                mHelper.getModelForWarningType(
                        PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        model.get(BUTTON_ACTION).run();

        histogram.assertExpected();
    }

    @Test
    public void inProductHelpForNoGmsCore() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(
                                        PasswordAccessLossWarningType.NO_GMS_CORE),
                                HELP_CENTER)
                        .build();

        PropertyModel model =
                mHelper.getModelForWarningType(PasswordAccessLossWarningType.NO_GMS_CORE);
        SpannableString text = model.get(SHEET_TEXT);
        clickSpan(text);

        histogram.assertExpected();
    }

    @Test
    public void inProductHelpForNoUpm() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(PasswordAccessLossWarningType.NO_UPM),
                                HELP_CENTER)
                        .build();

        PropertyModel model = mHelper.getModelForWarningType(PasswordAccessLossWarningType.NO_UPM);
        SpannableString text = model.get(SHEET_TEXT);
        clickSpan(text);

        histogram.assertExpected();
    }

    @Test
    public void inProductHelpForOnlyAccountUpm() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(
                                        PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM),
                                HELP_CENTER)
                        .build();

        PropertyModel model =
                mHelper.getModelForWarningType(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM);
        SpannableString text = model.get(SHEET_TEXT);
        clickSpan(text);

        histogram.assertExpected();
    }

    @Test
    public void sheetDismissalForFailedMigration() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(
                                        PasswordAccessLossWarningType
                                                .NEW_GMS_CORE_MIGRATION_FAILED),
                                DISMISS)
                        .build();

        mHelper.show(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, times(2)).addObserver(observerCaptor.capture());
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));

        notifyBottomSheetObservers(observerCaptor.getAllValues(), BACK_PRESS);

        histogram.assertExpected();
    }

    @Test
    public void sheetDismissalForNoGmsCore() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(
                                        PasswordAccessLossWarningType.NO_GMS_CORE),
                                DISMISS)
                        .build();

        mHelper.show(PasswordAccessLossWarningType.NO_GMS_CORE);
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, times(2)).addObserver(observerCaptor.capture());
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));

        notifyBottomSheetObservers(observerCaptor.getAllValues(), BACK_PRESS);

        histogram.assertExpected();
    }

    @Test
    public void sheetDismissalForNoUpm() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(PasswordAccessLossWarningType.NO_UPM),
                                DISMISS)
                        .build();

        mHelper.show(PasswordAccessLossWarningType.NO_UPM);
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, times(2)).addObserver(observerCaptor.capture());
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));

        notifyBottomSheetObservers(observerCaptor.getAllValues(), BACK_PRESS);

        histogram.assertExpected();
    }

    @Test
    public void sheetDismissalForOnlyAccountUpm() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getUserActionHistogramName(
                                        PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM),
                                DISMISS)
                        .build();

        mHelper.show(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM);
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, times(2)).addObserver(observerCaptor.capture());
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));

        notifyBottomSheetObservers(observerCaptor.getAllValues(), BACK_PRESS);

        histogram.assertExpected();
    }
}
