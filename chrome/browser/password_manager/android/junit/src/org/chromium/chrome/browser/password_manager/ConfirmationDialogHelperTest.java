// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertNotNull;

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
        mHelper.showConfirmation(
                /* title= */ "Title",
                /* message= */ "Message",
                /* confirmButtonTextId= */ R.string.ok,
                () -> {});
        assertNotNull(mModalDialogManager.getShownDialogModel());
    }
}
