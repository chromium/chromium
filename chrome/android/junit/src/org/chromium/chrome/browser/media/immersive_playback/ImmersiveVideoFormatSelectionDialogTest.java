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
import android.widget.RadioButton;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.ImmersivePlaybackConfirmationStatus;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ImmersiveVideoFormatSelectionDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoFormatSelectionDialogTest {
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ImmersivePlaybackConfirmationCallback mCallback;

    private Context mContext;
    private ImmersiveVideoFormatSelectionDialog mDialog;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mDialog = new ImmersiveVideoFormatSelectionDialog(mContext, mModalDialogManager, mCallback);
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

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(
                        modelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = modelCaptor.getValue();
        assertNotNull(model);

        // Dismiss with positive button click
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Default selected format is Standard (MONO / QUAD)
        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CONFIRMED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD);
    }

    @Test
    public void testPositiveButton_ChangeSelection() {
        mDialog.show();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(
                        modelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = modelCaptor.getValue();
        ImmersiveVideoFormatRadioGroup radioGroup =
                (ImmersiveVideoFormatRadioGroup) model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(radioGroup);

        // Change selection to the 3rd option (VR180)
        RadioButton radioButton = (RadioButton) radioGroup.getChildAt(2);
        radioButton.setChecked(true);

        // Dismiss with positive button click
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // VR180 options are MONO and HEMISPHERE
        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CONFIRMED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.HEMISPHERE);
    }

    @Test
    public void testNegativeButton_Declined() {
        mDialog.show();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(
                        modelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = modelCaptor.getValue();
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.DECLINED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD);
    }

    @Test
    public void testCancellation() {
        mDialog.show();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(
                        modelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = modelCaptor.getValue();
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CANCELED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD);
    }
}
