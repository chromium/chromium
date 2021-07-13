// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.DummyUiChromeActivityTestCase;
import org.chromium.ui.test.util.DummyUiActivity;

/**
 * Test for {@link ConfirmManagedSyncDataDialog}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(ConfirmSyncDataIntegrationTest.CONFIRM_SYNC_DATA_BATCH_NAME)
public class ConfirmManagedSyncDataDialogIntegrationTest extends DummyUiChromeActivityTestCase {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private ConfirmManagedSyncDataDialog.Listener mListenerMock;

    @Test
    @LargeTest
    public void testDialogIsDismissedWhenRecreated() throws Exception {
        ConfirmManagedSyncDataDialog dialog =
                ConfirmManagedSyncDataDialog.create(mListenerMock, TEST_DOMAIN);
        dialog.show(getActivity().getSupportFragmentManager(), null);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("The dialog should be visible!", dialog.getDialog().isShowing());
        DummyUiActivity activity = ApplicationTestUtils.recreateActivity(getActivity());
        Assert.assertNull("The dialog should be dismissed!", dialog.getDialog());
        ApplicationTestUtils.finishActivity(activity);
    }
}
