// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import static com.google.common.truth.Truth.assertThat;

import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class AddUsernameDialogModuleTest {
    private AddUsernameDialogController mController;
    private FakeModalDialogManager mModalDialogManager = new FakeModalDialogManager(0);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    AddUsernameDialogController.Delegate mBridgeDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mController = new AddUsernameDialogController(
                RuntimeEnvironment.getApplication(), mModalDialogManager, mBridgeDelegate);
    }

    @Test
    public void testDialogProperties() {
        mController.showAddUsernameDialog();

        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        Resources r = RuntimeEnvironment.getApplication().getResources();
        Assert.assertEquals(dialogModel.get(ModalDialogProperties.TITLE),
                r.getString(R.string.add_username_dialog_title));
        Assert.assertEquals(dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT),
                r.getString(R.string.add_username_dialog_add_username));
        Assert.assertEquals(dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT),
                r.getString(R.string.add_username_dialog_cancel));
    }

    @Test
    public void testDialogIsDismissed() {
        mController.showAddUsernameDialog();
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();

        mController.onClick(mModalDialogManager.getShownDialogModel(), ButtonType.NEGATIVE);
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
    }
}
