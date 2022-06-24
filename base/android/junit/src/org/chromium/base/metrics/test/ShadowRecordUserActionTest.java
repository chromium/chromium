// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics.test;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordUserAction;

import java.util.List;

/** Unit test for ShadowRecordUserAction. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = ShadowRecordUserAction.class)
public class ShadowRecordUserActionTest {
    @After
    public void tearDown() {
        ShadowRecordUserAction.reset();
    }

    @Test
    public void testRecord() {
        RecordUserAction.record("action1");
        RecordUserAction.record("action2");
        RecordUserAction.record("action3");
        RecordUserAction.record("action4");

        List<String> actions = ShadowRecordUserAction.getSamples();

        Assert.assertEquals("Size of the samples is different.", 4, actions.size());
        for (int i = 0; i < 4; ++i) {
            Assert.assertEquals("Sample order is different.", "action" + (i + 1), actions.get(i));
        }

        ShadowRecordUserAction.reset();
        Assert.assertTrue("Use actions should be cleared after reset.",
                ShadowRecordUserAction.getSamples().isEmpty());
    }

    @Test(expected = AssertionError.class)
    public void testSetActionCallbackForTesting() {
        RecordUserAction.setActionCallbackForTesting(action -> {});
    }

    @Test(expected = AssertionError.class)
    public void testRemoveActionCallbackForTesting() {
        RecordUserAction.removeActionCallbackForTesting();
    }
}
