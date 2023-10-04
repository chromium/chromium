// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.base.ThreadUtils;

/** Wrapper around AwActivityTestRule with helper methods for tests using multiple profiles. */
public class MultiProfileTestRule extends AwActivityTestRule {
    private final TestAwContentsClient mContentsClient;

    public MultiProfileTestRule() {
        mContentsClient = new TestAwContentsClient();
    }

    public AwContents createAwContents() {
        return createAwTestContainerViewOnMainSync(mContentsClient).getAwContents();
    }

    public AwContents createAwContents(AwBrowserContext browserContext) {
        AwContents awContents =
                createAwTestContainerViewOnMainSync(mContentsClient).getAwContents();
        setBrowserContextSync(awContents, browserContext);
        return awContents;
    }

    public void setBrowserContextSync(AwContents awContents, AwBrowserContext browserContext) {
        ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            awContents.setBrowserContext(browserContext);
            return null;
        });
    }

    public AwBrowserContext getProfileSync(String name, boolean createIfNeeded) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> AwBrowserContext.getNamedContext(name, createIfNeeded));
    }

    public TestAwContentsClient getContentsClient() {
        return mContentsClient;
    }
}
