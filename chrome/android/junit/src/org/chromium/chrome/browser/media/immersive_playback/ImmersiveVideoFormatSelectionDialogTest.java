// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid.ImmersivePlaybackConfirmationCallback;
import org.chromium.content_public.browser.ImmersivePlaybackConfirmationStatus;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ImmersiveVideoFormatSelectionDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImmersiveVideoFormatSelectionDialogTest {
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ImmersivePlaybackConfirmationCallback mCallback;
    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    private Context mContext;
    private ImmersiveVideoFormatSelectionDialog mDialog;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mDialog =
                new ImmersiveVideoFormatSelectionDialog(
                        mContext, mModalDialogManager, mCallback::onResult);
    }

    @Test
    public void testShow() {
        mDialog.show();

        verify(mModalDialogManager)
                .showDialog(
                        any(PropertyModel.class),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));
    }

    @Test
    public void testPositiveButton_DefaultSelection() {
        mDialog.show();

        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        assertNotNull(model);

        // Dismiss with positive button click
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Default selected format is Standard (MONO / QUAD)
        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CONFIRMED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD,
                        false);
    }

    @Test
    public void testPositiveButton_ChangeSelection() {
        mDialog.show();

        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        ImmersiveVideoFormatRadioGroup radioGroup =
                (ImmersiveVideoFormatRadioGroup) model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(radioGroup);

        // Change selection to 180° stereoscopic
        radioGroup.checkOption(
                ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveProjectionType.HEMISPHERE);

        // Dismiss with positive button click
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // 180° stereoscopic options are SIDE_BY_SIDE and HEMISPHERE
        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CONFIRMED,
                        ImmersiveStereoMode.SIDE_BY_SIDE,
                        ImmersiveProjectionType.HEMISPHERE,
                        false);
    }

    @Test
    public void testNegativeButton_Declined() {
        mDialog.show();

        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.DECLINED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD,
                        false);
    }

    @Test
    public void testCancellation() {
        mDialog.show();

        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CANCELED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD,
                        false);
    }
}
