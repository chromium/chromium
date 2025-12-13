// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static android.app.Activity.RESULT_OK;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Dialog;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;

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
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.test.BrowserUiTestFragmentActivity;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link PasswordsCsvDownloadDialogController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordCsvDownloadDialogTest {
    private static final Uri SAVED_EXPORT_FILE_URI = Uri.parse("fake/test/path/file.ext");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PasswordCsvDownloadDialogController mController;

    private FragmentActivity mActivity;

    @Mock private SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    @Before
    public void setUp() {
        mActivity =
                Robolectric.buildActivity(BrowserUiTestFragmentActivity.class)
                        .create()
                        .start()
                        .resume()
                        .get();
        mActivity.setTheme(
                org.chromium.components.browser_ui.test.R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testDialogContentsWithGms() {
        mController =
                new PasswordCsvDownloadDialogController(
                        mActivity,
                        true,
                        () -> {},
                        () -> {},
                        mSettingsCustomTabLauncher,
                        (Uri uri) -> {});
        mController.showDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Resources resources = RuntimeEnvironment.getApplication().getResources();
        Dialog dialog = ShadowDialog.getLatestDialog();

        assertEquals(
                resources.getString(R.string.keep_access_to_your_passwords_dialog_title),
                ((TextView) dialog.findViewById(R.id.title)).getText());

        SpannableString expectedDetailsParagraph1 =
                SpanApplier.applySpans(
                        resources.getString(R.string.csv_download_dialog_with_gms_paragraph1),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(mActivity, (view) -> {})));
        assertEquals(
                expectedDetailsParagraph1.toString(),
                ((TextView) dialog.findViewById(R.id.details_paragraph1)).getText().toString());
        assertEquals(
                resources.getString(R.string.csv_download_dialog_paragraph2),
                ((TextView) dialog.findViewById(R.id.details_paragraph2)).getText());
        assertEquals(
                resources.getString(R.string.csv_download_dialog_positive_button_text),
                ((TextView) dialog.findViewById(R.id.positive_button)).getText());
        assertEquals(
                resources.getString(R.string.cancel),
                ((TextView) dialog.findViewById(R.id.negative_button)).getText());
    }

    @Test
    public void testDialogContentsNoGms() {
        mController =
                new PasswordCsvDownloadDialogController(
                        mActivity,
                        false,
                        () -> {},
                        () -> {},
                        mSettingsCustomTabLauncher,
                        (Uri uri) -> {});
        mController.showDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Resources resources = RuntimeEnvironment.getApplication().getResources();
        Dialog dialog = ShadowDialog.getLatestDialog();

        assertEquals(
                resources.getString(R.string.keep_access_to_your_passwords_dialog_title),
                ((TextView) dialog.findViewById(R.id.title)).getText());
        assertEquals(
                resources.getString(R.string.csv_download_dialog_no_gms_paragraph1),
                ((TextView) dialog.findViewById(R.id.details_paragraph1)).getText().toString());
        assertEquals(
                resources.getString(R.string.csv_download_dialog_paragraph2),
                ((TextView) dialog.findViewById(R.id.details_paragraph2)).getText());
        assertEquals(
                resources.getString(R.string.csv_download_dialog_positive_button_text),
                ((TextView) dialog.findViewById(R.id.positive_button)).getText());
        assertEquals(
                resources.getString(R.string.cancel),
                ((TextView) dialog.findViewById(R.id.negative_button)).getText());
    }

    @Test
    public void testPositiveButtonClick() {
        Runnable positiveButtonCalback = mock(Runnable.class);
        mController =
                new PasswordCsvDownloadDialogController(
                        mActivity,
                        false,
                        positiveButtonCalback,
                        () -> {},
                        mSettingsCustomTabLauncher,
                        (Uri uri) -> {});
        mController.showDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();
        verify(positiveButtonCalback).run();
    }

    @Test
    public void testNegativeButtonClick() {
        Runnable negativeButtonCalback = mock(Runnable.class);
        mController =
                new PasswordCsvDownloadDialogController(
                        mActivity,
                        false,
                        () -> {},
                        negativeButtonCalback,
                        mSettingsCustomTabLauncher,
                        (Uri uri) -> {});
        mController.showDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.negative_button).performClick();
        verify(negativeButtonCalback).run();
    }

    @Test
    public void testHelpLinkClick() {
        mController =
                new PasswordCsvDownloadDialogController(
                        mActivity,
                        true,
                        () -> {},
                        () -> {},
                        mSettingsCustomTabLauncher,
                        (Uri uri) -> {});
        mController.showDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        TextViewWithClickableSpans textView = dialog.findViewById(R.id.details_paragraph1);
        ClickableSpan[] clickableSpans = textView.getClickableSpans();
        assertEquals(1, clickableSpans.length);
        clickableSpans[0].onClick(textView);
        verify(mSettingsCustomTabLauncher).openUrlInCct(any(), any());
    }

    @Test
    public void testOpensDocumentCreationAndReturnsUri() {
        final AtomicReference<Uri> uri = new AtomicReference<>();
        mController =
                new PasswordCsvDownloadDialogController(
                        mActivity,
                        false,
                        () -> {},
                        () -> {},
                        mSettingsCustomTabLauncher,
                        (destinationUri) -> {
                            uri.set(destinationUri);
                        });
        mController.showDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        mController.askForDownloadLocation();

        ShadowActivity shadowActivity = shadowOf(mActivity);
        Intent startedIntent = shadowActivity.getNextStartedActivityForResult().intent;
        // Verify that the create document intent was triggered (creating file in Downloads for
        // exported passwords).
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, startedIntent.getAction());

        // Return the result of the create document intent (the file name).
        shadowActivity.receiveResult(
                startedIntent, RESULT_OK, new Intent().setData(SAVED_EXPORT_FILE_URI));
        assertEquals(SAVED_EXPORT_FILE_URI, uri.get());
    }
}
