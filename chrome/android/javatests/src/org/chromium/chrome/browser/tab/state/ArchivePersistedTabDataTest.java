// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;

import java.nio.ByteBuffer;
import java.util.concurrent.TimeoutException;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
public class ArchivePersistedTabDataTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
    }

    @SmallTest
    @Test
    public void testEmpty() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ArchivePersistedTabData.from(
                            mActivityTestRule.getActivity().getActivityTab(),
                            (res) -> {
                                Assert.assertNotNull(res);
                                Assert.assertEquals(
                                        ArchivePersistedTabData.INVALID_TIMESTAMP,
                                        res.getArchivedTimeMs());
                                helper.notifyCalled();
                            });
                });
        helper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testRestore() throws TimeoutException {
        CallbackHelper[] helpers = new CallbackHelper[3];
        helpers[0] = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ArchivePersistedTabData archivePersistedTabData =
                            ArchivePersistedTabData.from(
                                    mActivityTestRule.getActivity().getActivityTab());
                    ObservableSupplierImpl<Boolean> observableSupplier =
                            new ObservableSupplierImpl<>();
                    observableSupplier.set(true);
                    archivePersistedTabData.registerIsTabSaveEnabledSupplier(observableSupplier);
                    archivePersistedTabData.setArchivedTimeMs(42L);
                    helpers[0].notifyCalled();
                });
        helpers[0].waitForCallback(0);
        helpers[1] = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getActivityTab()
                            .getUserDataHost()
                            .removeUserData(ArchivePersistedTabData.class);
                    helpers[1].notifyCalled();
                });
        helpers[1].waitForCallback(0);
        helpers[2] = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ArchivePersistedTabData.from(
                            mActivityTestRule.getActivity().getActivityTab(),
                            (res) -> {
                                Assert.assertNotNull(res);
                                Assert.assertEquals(42L, res.getArchivedTimeMs());
                                helpers[2].notifyCalled();
                            });
                });
        helpers[2].waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testDeserializationFailure() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ArchivePersistedTabData.from(
                            mActivityTestRule.getActivity().getActivityTab(),
                            (res) -> {
                                Assert.assertNotNull(res);
                                ByteBuffer bytes = null;
                                Assert.assertFalse(
                                        "Null byte buffer should early return",
                                        res.deserialize(bytes));
                                bytes = ByteBuffer.allocate(0);
                                Assert.assertFalse(
                                        "Empty byte buffer should early return",
                                        res.deserialize(bytes));
                                bytes = ByteBuffer.allocate(100);
                                Assert.assertFalse(
                                        "Non-empty byte buffer which isn't a proto should throw a"
                                                + " caught exception and return",
                                        res.deserialize(bytes));
                                Assert.assertFalse(res.deserialize(bytes));
                                helper.notifyCalled();
                            });
                });
        helper.waitForCallback(0);
    }
}
