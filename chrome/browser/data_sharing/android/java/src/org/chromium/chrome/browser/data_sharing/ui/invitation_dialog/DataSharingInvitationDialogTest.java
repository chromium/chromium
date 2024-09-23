// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.invitation_dialog;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeoutException;

/** Test relating to {@link DataSharingInvitationDialogCoordinator} */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DataSharingInvitationDialogTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static BlankUiTestActivity sActivity;
    @Mock private ModalDialogManagerObserver mObserver;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sActivity = runOnUiThreadBlocking(() -> sActivityTestRule.getActivity());
    }

    @Before
    public void setupTest() throws Exception {
        MockitoAnnotations.initMocks(this);
        runOnUiThreadBlocking(() -> sActivity.getModalDialogManager().addObserver(mObserver));
    }

    @After
    public void teardownTest() throws Exception {
        runOnUiThreadBlocking(() -> sActivity.getModalDialogManager().removeObserver(mObserver));
    }

    @Test
    @MediumTest
    public void testShownAndDismissed() throws TimeoutException {
        DataSharingInvitationDialogCoordinator coordinator =
                runOnUiThreadBlocking(
                        () ->
                                new DataSharingInvitationDialogCoordinator(
                                        sActivity, sActivity.getModalDialogManager()));

        verify(mObserver, times(0)).onDialogAdded(any());

        runOnUiThreadBlocking(coordinator::show);
        verify(mObserver, times(1)).onDialogAdded(any());

        runOnUiThreadBlocking(coordinator::dismiss);
        verify(mObserver, times(1)).onDialogDismissed(any());
    }
}
