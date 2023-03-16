// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/**
 * Unit tests for Quick Delete dialog.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class QuickDeleteDialogDelegateUnitTest {
    @Mock
    private Callback<Integer> mOnDismissCallback;

    private FakeModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        new QuickDeleteDialogDelegate(activity, mModalDialogManager, mOnDismissCallback)
                .showDialog();
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mOnDismissCallback);
    }

    @Test
    @SmallTest
    public void testCancelQuickDelete() {
        mModalDialogManager.clickNegativeButton();
        verify(mOnDismissCallback, times(1)).onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testConfirmQuickDelete() {
        mModalDialogManager.clickPositiveButton();
        verify(mOnDismissCallback, times(1)).onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }
}
