// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.gfx.AwDrawFnImpl;

/**
 * A test dependency factory that creates TestAwContents instances instead of regular AwContents.
 * This allows tests to use the enhanced TestAwContents class which provides additional testing
 * capabilities.
 */
public class TestAwContentsClientTestDependencyFactory
        extends AwActivityTestRule.TestDependencyFactory {
    @Override
    public AwContents createAwContents(
            AwBrowserContext browserContext,
            ViewGroup containerView,
            Context context,
            InternalAccessDelegate internalAccessAdapter,
            AwDrawFnImpl.DrawFnAccess drawFnAccess,
            AwContentsClient contentsClient,
            AwSettings settings,
            DependencyFactory dependencyFactory) {
        return new TestAwContents(
                browserContext,
                containerView,
                context,
                internalAccessAdapter,
                drawFnAccess,
                contentsClient,
                settings,
                dependencyFactory);
    }
}
