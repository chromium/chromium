// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.MockitoAnnotations.initMocks;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Test for {@link ConfirmManagedSyncDataDialog}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(ConfirmSyncDataIntegrationTest.CONFIRM_SYNC_DATA_BATCH_NAME)
public class ConfirmManagedSyncDataDialogIntegrationTest extends DummyUiActivityTestCase {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Mock
    private ConfirmManagedSyncDataDialog.Listener mListenerMock;

    @Before
    public void setUp() {
        initMocks(this);
    }

    @Test
    @LargeTest
    public void testDialogIsDismissedWhenRecreated() {
        ConfirmManagedSyncDataDialog dialog =
                ConfirmManagedSyncDataDialog.create(mListenerMock, TEST_DOMAIN);
        DummyUiActivity activity = getActivity();
        dialog.show(activity.getSupportFragmentManager(), null);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("The dialog should be visible!", dialog.getDialog().isShowing());
        ApplicationTestUtils.recreateActivity(activity);
        Assert.assertNull("The dialog should be dismissed!", dialog.getDialog());
    }
}
