// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.ActivityManager;
import android.os.Build;
import android.util.Pair;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.util.ArrayList;
import java.util.List;

/** Tests for BinderCallsListener. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BinderCallsListenerTest {
    @Before
    @After
    public void cleanup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BinderCallsListener.getInstance().setBinderCallListenerObserverForTesting(null);
                });
        BinderCallsListener.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCanObserveFrameworkCalls() {
        BinderCallsListener listener = BinderCallsListener.getInstance();
        boolean success = listener.installListener();

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            // The API was added in Android 10, but don't skip the test in earlier releases, to
            // check that we don't cause crashes there.
            Assert.assertFalse(success);
            return;
        }
        Assert.assertTrue(success);
        List<Pair<String, String>> actions = new ArrayList<>();
        listener.setBinderCallListenerObserverForTesting(
                (String action, String caller) -> {
                    actions.add(new Pair<>(action, caller));
                });
        ActivityManager.RunningAppProcessInfo state = new ActivityManager.RunningAppProcessInfo();
        ActivityManager.getMyMemoryState(state);
        Assert.assertEquals(2, actions.size());
        Assert.assertEquals("onTransactStarted", actions.get(0).first);
        Assert.assertEquals("android.app.IActivityManager", actions.get(0).second);
        Assert.assertEquals("onTransactEnded", actions.get(1).first);
        Assert.assertEquals("android.app.IActivityManager", actions.get(1).second);
    }
}
