// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link AutofillAiSaveUpdateEntityPrompt}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillAiSaveUpdateEntityPromptTest {
    private static final long NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER = 100L;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillAiSaveUpdateEntityPromptController.Natives mPromptControllerJni;

    private AutofillAiSaveUpdateEntityPromptController mPromptController;
    private AutofillAiSaveUpdateEntityPrompt mPrompt;
    private FakeModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        AutofillAiSaveUpdateEntityPromptControllerJni.setInstanceForTesting(mPromptControllerJni);
        mPromptController =
                AutofillAiSaveUpdateEntityPromptController.create(
                        NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mPrompt =
                new AutofillAiSaveUpdateEntityPrompt(
                        mPromptController, mModalDialogManager, RuntimeEnvironment.application);
    }

    @Test
    @SmallTest
    public void userAccepted() {
        mPrompt.show();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mModalDialogManager.clickPositiveButton();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni, times(1))
                .onUserAccepted(eq(NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER));
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER));
    }

    @Test
    @SmallTest
    public void userDeclined() {
        mPrompt.show();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni, times(1))
                .onUserDeclined(eq(NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER));
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER));
    }

    @Test
    @SmallTest
    public void promptDismissed() {
        mPrompt.show();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mPrompt.dismiss();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER));
    }

    @Test
    @SmallTest
    public void dialogStrings() {
        mPrompt.setDialogDetails("title", "positive button text", "negative button text");
        mPrompt.show();
        PropertyModel propertyModel = mModalDialogManager.getShownDialogModel();

        assertEquals("title", propertyModel.get(ModalDialogProperties.TITLE));
        assertEquals(
                "positive button text",
                propertyModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                "negative button text",
                propertyModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }
}
