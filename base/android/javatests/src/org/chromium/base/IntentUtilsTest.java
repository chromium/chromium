// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.build.BuildConfig;

/** Tests for {@link IntentUtils}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class IntentUtilsTest {
    @Mock private Context mContext;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private void assertTargetsSelf(boolean targetsSelf, Intent intent, boolean expectAssertion) {
        boolean asserted = false;
        try {
            Assert.assertEquals(targetsSelf, IntentUtils.intentTargetsSelf(mContext, intent));
        } catch (AssertionError e) {
            asserted = true;
            if (!expectAssertion) throw e;
        }
        if (BuildConfig.ENABLE_ASSERTS) Assert.assertEquals(expectAssertion, asserted);
    }

    @Test
    @SmallTest
    public void testIntentTargetsSelf() {
        String packageName = "package.name";
        Mockito.when(mContext.getPackageName()).thenReturn(packageName);
        assertTargetsSelf(false, new Intent(), false);
        assertTargetsSelf(true, new Intent(mContext, IntentUtilsTest.class), false);

        Intent intent = new Intent();
        intent.setComponent(new ComponentName(packageName, ""));
        assertTargetsSelf(true, intent, false);

        intent.setComponent(
                new ComponentName("other.package", "org.chromium.base.IntentUtilsTest"));
        assertTargetsSelf(false, intent, false);

        intent.setPackage(packageName);
        assertTargetsSelf(false, intent, true);

        intent.setComponent(null);
        assertTargetsSelf(true, intent, false);

        intent.setPackage("other.package");
        assertTargetsSelf(false, intent, false);

        intent.setComponent(new ComponentName(packageName, ""));
        assertTargetsSelf(false, intent, true);

        intent.setPackage(null);
        assertTargetsSelf(true, intent, false);
    }
}
