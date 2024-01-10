// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.DialogInterface;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Tests for {@link ConfirmationDialogHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class ConfirmationDialogHelperTest {
    private Activity mActivity;
    private FakeModalDialogManager mModalDialogManager;
    private ConfirmationDialogHelper mHelper;

    @Mock private DialogInterface mDialogInterface;
    @Mock private Runnable mConfirmedCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.setupActivity(Activity.class);
        mHelper = new ConfirmationDialogHelper(mActivity);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mHelper.setModalDialogManagerForTesting(mModalDialogManager);
    }

    @Test
    @SmallTest
    public void dialogShown() {
        mHelper.showConfirmation("Title", "Message", R.string.ok, mConfirmedCallback);
        assertNotNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    @SmallTest
    public void positiveButtonPressed() {
        mHelper.showConfirmation("Title", "Message", R.string.ok, mConfirmedCallback);
        mModalDialogManager.clickPositiveButton();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mConfirmedCallback, times(1)).run();
    }

    @Test
    @SmallTest
    public void negativeButtonPressed() {
        mHelper.showConfirmation("Title", "Message", R.string.ok, mConfirmedCallback);
        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mConfirmedCallback, times(0)).run();
    }

    @Test
    @SmallTest
    public void dialogStrings() {
        mHelper.showConfirmation("Title", "Message", R.string.ok, mConfirmedCallback);
        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertNotNull(model);
        assertEquals(model.get(ModalDialogProperties.TITLE), "Title");
        assertEquals(model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1), "Message");
        assertEquals(
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT),
                mActivity.getString(R.string.ok));
        assertEquals(
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT),
                mActivity.getString(R.string.cancel));
    }
}
