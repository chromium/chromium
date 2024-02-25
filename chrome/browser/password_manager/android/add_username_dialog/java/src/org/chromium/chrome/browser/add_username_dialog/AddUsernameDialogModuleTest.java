// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;

import android.content.res.Resources;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.textfield.TextInputEditText;

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
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
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
    private AppCompatActivity mActivity;
    private static final String TEST_PASSWORD = "password";
    private static final String TEST_USERNAME = "username";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock AddUsernameDialogController.Delegate mBridgeDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mActivity = createActivity();
        mController =
                new AddUsernameDialogController(mActivity, mModalDialogManager, mBridgeDelegate);
    }

    private static AppCompatActivity createActivity() {
        ActivityController<AppCompatActivity> activityController =
                Robolectric.buildActivity(AppCompatActivity.class);
        // Need to setTheme to Activity in Robolectric test or will get exception: You need to use a
        // Theme.AppCompat theme (or descendant) with this activity.
        activityController.get().setTheme(R.style.Theme_AppCompat_Light);
        return activityController.create().start().resume().visible().get();
    }

    @Test
    public void testDialogProperties() {
        mController.showAddUsernameDialog(TEST_PASSWORD);

        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        Resources r = mActivity.getResources();
        Assert.assertEquals(
                dialogModel.get(ModalDialogProperties.TITLE),
                r.getString(R.string.add_username_dialog_title));
        Assert.assertEquals(
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT),
                r.getString(R.string.add_username_dialog_add_username));
        Assert.assertEquals(
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT),
                r.getString(R.string.add_username_dialog_cancel));

        TextInputEditText usernameInput =
                dialogModel.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.username);
        TextInputEditText passwordInput =
                dialogModel.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.password);
        Assert.assertTrue(usernameInput.getText().length() == 0);
        Assert.assertTrue(usernameInput.isFocused());
        Assert.assertEquals(passwordInput.getText().toString(), TEST_PASSWORD);
    }

    @Test
    public void testDialogIsDismissed() {
        mController.showAddUsernameDialog(TEST_PASSWORD);
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();

        mController.onClick(mModalDialogManager.getShownDialogModel(), ButtonType.NEGATIVE);
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        verify(mBridgeDelegate).onDialogDismissed();
    }

    @Test
    public void testDialogIsAccepted() {
        mController.showAddUsernameDialog(TEST_PASSWORD);
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        TextInputEditText usernameInput =
                dialogModel.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.username);
        usernameInput.setText(TEST_USERNAME);

        mController.onClick(mModalDialogManager.getShownDialogModel(), ButtonType.POSITIVE);
        verify(mBridgeDelegate).onDialogAccepted(TEST_USERNAME);
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        verify(mBridgeDelegate).onDialogDismissed();
    }
}
