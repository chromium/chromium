// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.content_public.common.ContentSwitches;

/**
 * Tests for WebView Prefetch API using the off the main thread (OMT) trigger, with OMT Prefetch
 * *disabled*, i.e. `AwPrefetchManager.startPrefetchRequestAsync()` is called but rerouted to the
 * main-thread prefetch path. See the class comment of `AwPrefetchTestBase`.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
@CommandLineFlags.Add({
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "disable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
})
public class AwPrefetchOmtDisabledTest extends AwPrefetchOmtTestBase {
    public AwPrefetchOmtDisabledTest(AwSettingsMutation param) {
        super(param);
    }
}
