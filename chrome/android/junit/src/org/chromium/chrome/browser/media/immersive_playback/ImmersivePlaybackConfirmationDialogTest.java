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
import android.widget.Spinner;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.ImmersivePlaybackConfirmationStatus;
import org.chromium.blink.mojom.ImmersiveProjectionType;
import org.chromium.blink.mojom.ImmersiveStereoMode;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ImmersivePlaybackConfirmationDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersivePlaybackConfirmationDialogTest {
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ImmersivePlaybackConfirmationDialog.ConfirmationCallback mCallback;

    private Context mContext;
    private ImmersivePlaybackConfirmationDialog mDialog;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mDialog = new ImmersivePlaybackConfirmationDialog(mContext, mModalDialogManager, mCallback);
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
    public void testPositiveButton() {
        mDialog.show();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(
                        modelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = modelCaptor.getValue();
        assertNotNull(model);

        // Obtain internal spinners directly from the exposed dialog instances.
        Spinner stereoSpinner = mDialog.mStereoSpinner;
        Spinner projectionSpinner = mDialog.mProjectionSpinner;

        assertNotNull(stereoSpinner);
        assertNotNull(projectionSpinner);

        // Set the last items dynamically to ensure we're selecting from current capacity
        int stereoIndex = Math.max(0, stereoSpinner.getCount() - 1);
        int projectionIndex = Math.max(0, projectionSpinner.getCount() - 1);
        stereoSpinner.setSelection(stereoIndex);
        projectionSpinner.setSelection(projectionIndex);

        // Manually trigger dismissal through the controller to simulate ModalDialogManager
        // lifecycle
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Verify results map to class defined constant keys.
        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CONFIRMED,
                        ImmersivePlaybackConfirmationDialog.STEREO_MODE_KEYS[stereoIndex],
                        ImmersivePlaybackConfirmationDialog.PROJECTION_TYPE_KEYS[projectionIndex]);
    }

    @Test
    public void testNegativeButton() {
        mDialog.show();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(
                        modelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = modelCaptor.getValue();

        // Manually trigger dismissal through the controller to simulate ModalDialogManager
        // lifecycle
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

        // Cancel dialog
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CANCELED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD);
    }

    @Test
    public void testDismiss() {
        mDialog.show();
        mDialog.dismiss();

        verify(mModalDialogManager)
                .dismissDialog(any(), eq(DialogDismissalCause.DISMISSED_BY_NATIVE));
    }
}
