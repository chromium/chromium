// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.content.Context;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.FeatureParam;
import org.chromium.base.Flag;
import org.chromium.base.test.BaseJUnit4ClassRunner.TestHook;

/** Resets any cached values held by active {@link Flag} instances. */
public class ResetCachedFlagValuesTestHook implements TestHook {
    @Override
    public void run(Context targetContext, FrameworkMethod testMethod) {
        Flag.resetAllInMemoryCachedValuesForTesting();
        FeatureParam.resetAllInMemoryCachedValuesForTesting();
    }
}
