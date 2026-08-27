// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.base.test.util.DoNotBatch;

/**
 * Tests for WebView Prefetch API.
 *
 * <p>TODO(crbug.com/544544669): Currently this is a test class for ALL WebView Prefetch tests, but
 * happens to be named as `AwPrefetchMTTest` just to tweak git log history simpler. Subsequent CL
 * https://crrev.com/c/8252962 will soon make this `AwPrefetchMTTest` class for Main-Thread
 * scenarios only.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwPrefetchMTTest extends AwPrefetchTestBase {
    public AwPrefetchMTTest(AwSettingsMutation param) {
        super(param);
    }
}
