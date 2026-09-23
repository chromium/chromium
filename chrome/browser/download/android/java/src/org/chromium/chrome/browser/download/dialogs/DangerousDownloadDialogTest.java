// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.dialogs.DangerousDownloadDialog.DangerousDownloadDialogEvent;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Tests for {@link DangerousDownloadDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DangerousDownloadDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Callback<Integer> mResultCallback;

    private Context mContext;
    private Resources mResources;
    private DangerousDownloadDialog mDialog;
    private PropertyModel mModalDialogModel;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mResources = mContext.getResources();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
    public void testDialogProperties_dangerous() {
        createAndShowDialog(/* isDangerous= */ true);

        Assert.assertEquals(
                "Dialog title should be dangerous title.",
                mResources.getString(R.string.dangerous_download_dialog_title),
                mModalDialogModel.get(ModalDialogProperties.TITLE));
        Assert.assertEquals(
                "Positive button text should be confirm.",
                mResources.getString(R.string.dangerous_download_dialog_confirm_text),
                mModalDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals(
                "Negative button text should be cancel.",
                mResources.getString(R.string.cancel),
                mModalDialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        Assert.assertFalse(
                "App modal dialog should not cancel on touch outside.",
                mModalDialogModel.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        Assert.assertNotNull(
                "App modal dialog should set a back press handler.",
                mModalDialogModel.get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
    public void testDialogProperties_nonDangerous() {
        createAndShowDialog(/* isDangerous= */ false);

        Assert.assertEquals(
                "Dialog title should be non-dangerous title.",
                mResources.getString(R.string.non_dangerous_download_dialog_title),
                mModalDialogModel.get(ModalDialogProperties.TITLE));
        Assert.assertEquals(
                "Positive button text should be confirm.",
                mResources.getString(R.string.non_dangerous_download_dialog_confirm_text),
                mModalDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals(
                "Negative button text should be cancel.",
                mResources.getString(R.string.cancel),
                mModalDialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
    public void testPositiveButton_acceptsDownload() {
        createAndShowDialog(/* isDangerous= */ true);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mResultCallback)
                .onResult(DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_CONFIRM);
        verify(mModalDialogManager)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        dialogController.onDismiss(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mResultCallback)
                .onResult(DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_CONFIRM);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
    public void testNegativeButton_cancelsDownload() {
        createAndShowDialog(/* isDangerous= */ true);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mResultCallback)
                .onResult(DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_CANCEL);
        verify(mModalDialogManager)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
    public void testDismissal_lifecycleOrTabSwitched_reportsDismiss() {
        createAndShowDialog(/* isDangerous= */ true);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        // Simulate a dismissal from tab switching or lifecycle event.
        dialogController.onDismiss(mModalDialogModel, DialogDismissalCause.TAB_SWITCHED);
        verify(mResultCallback)
                .onResult(DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_DISMISS);
        verify(mResultCallback, never())
                .onResult(DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_CANCEL);
        verify(mResultCallback, never())
                .onResult(DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_CONFIRM);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
    public void testDialogMessageParagraphs_withDomainAndSize() {
        mDialog = new DangerousDownloadDialog();
        mDialog.show(
                mContext,
                mModalDialogManager,
                "test.apk",
                1024,
                "example.com",
                0,
                mResultCallback,
                /* isDangerous= */ true);

        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(captor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
        PropertyModel model = captor.getValue();
        List<CharSequence> paragraphs = model.get(ModalDialogProperties.MESSAGE_PARAGRAPHS);
        Assert.assertNotNull(paragraphs);
        Assert.assertEquals(1, paragraphs.size());
        Assert.assertTrue(paragraphs.get(0).toString().contains("example.com"));
        Assert.assertTrue(paragraphs.get(0).toString().contains("test.apk"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
    public void testDialogMessageParagraphs_withoutDomain() {
        mDialog = new DangerousDownloadDialog();
        mDialog.show(
                mContext,
                mModalDialogManager,
                "test.apk",
                1024,
                "",
                0,
                mResultCallback,
                /* isDangerous= */ true);

        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(captor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
        PropertyModel model = captor.getValue();
        List<CharSequence> paragraphs = model.get(ModalDialogProperties.MESSAGE_PARAGRAPHS);
        Assert.assertNotNull(paragraphs);
        Assert.assertEquals(1, paragraphs.size());
        Assert.assertFalse(paragraphs.get(0).toString().contains("example.com"));
        Assert.assertTrue(paragraphs.get(0).toString().contains("test.apk"));
    }

    private void createAndShowDialog(boolean isDangerous) {
        mDialog = new DangerousDownloadDialog();
        mDialog.show(
                mContext,
                mModalDialogManager,
                "test.apk",
                1024,
                "example.com",
                0,
                mResultCallback,
                isDangerous);

        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(captor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
        mModalDialogModel = captor.getValue();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.MALICIOUS_APK_DOWNLOAD_CHECK)
    public void testShowDialog_featureDisabled_usesTabDialogType() {
        mDialog = new DangerousDownloadDialog();
        mDialog.show(
                mContext,
                mModalDialogManager,
                "test.apk",
                1024,
                "example.com",
                0,
                mResultCallback,
                /* isDangerous= */ true);

        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(captor.capture(), eq(ModalDialogManager.ModalDialogType.TAB));
    }
}
