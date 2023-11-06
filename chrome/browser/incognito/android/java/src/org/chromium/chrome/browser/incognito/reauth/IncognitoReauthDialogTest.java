// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;

import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Robolectric tests for {@link IncognitoReauthDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Batch(UNIT_TESTS)
public class IncognitoReauthDialogTest {
    @Mock private ModalDialogManager mModalDialogManagerMock;
    @Mock private View mIncognitoReauthViewMock;

    private OnBackPressedCallback mOnBackPressedCallbackMock =
            new OnBackPressedCallback(false) {
                @Override
                public void handleOnBackPressed() {}
            };

    private IncognitoReauthDialog mIncognitoReauthDialog;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mIncognitoReauthDialog =
                new IncognitoReauthDialog(
                        mModalDialogManagerMock,
                        mIncognitoReauthViewMock,
                        mOnBackPressedCallbackMock);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mModalDialogManagerMock, mIncognitoReauthViewMock);
    }

    @Test
    @SmallTest
    public void testPropertyModelAttributes_CorrectlySet() {
        PropertyModel model = mIncognitoReauthDialog.getModalDialogPropertyModelForTesting();
        assertEquals(
                "View mis-match! Supplied view can't be different from the custom view.",
                mIncognitoReauthViewMock,
                model.get(ModalDialogProperties.CUSTOM_VIEW));
        assertFalse(
                "Interactions done outside the re-auth dialog must be ignored.",
                model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        assertTrue(
                "re-auth dialog must be dark full-screen style.",
                model.get(ModalDialogProperties.DIALOG_STYLES)
                        == ModalDialogProperties.DialogStyles.FULLSCREEN_DARK_DIALOG);
    }

    @Test
    @SmallTest
    public void testShowIncognitoReauthDialog_Invokes_ModalDiaogManager() {
        PropertyModel model = mIncognitoReauthDialog.getModalDialogPropertyModelForTesting();
        doNothing()
                .when(mModalDialogManagerMock)
                .showDialog(
                        model,
                        ModalDialogManager.ModalDialogType.APP,
                        ModalDialogManager.ModalDialogPriority.VERY_HIGH);

        mIncognitoReauthDialog.showIncognitoReauthDialog();

        verify(mModalDialogManagerMock)
                .showDialog(
                        model,
                        ModalDialogManager.ModalDialogType.APP,
                        ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    @Test
    @SmallTest
    public void testdismissIncognitoReauthDialog_InvokesModalDiaogManager() {
        PropertyModel model = mIncognitoReauthDialog.getModalDialogPropertyModelForTesting();
        doNothing()
                .when(mModalDialogManagerMock)
                .dismissDialog(model, DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);

        mIncognitoReauthDialog.dismissIncognitoReauthDialog(
                DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);

        verify(mModalDialogManagerMock)
                .dismissDialog(model, DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);
    }
}
