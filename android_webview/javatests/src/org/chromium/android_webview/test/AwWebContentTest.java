// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import android.content.Context;

import androidx.test.filters.SmallTest;

import com.android.webview.chromium.WebContentContextWrapper;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwWebContent;
import org.chromium.android_webview.StartupController;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.ContextUtils;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link AwWebContent}. */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AwWebContentTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDestroyWhileStartupIsDeferred() {
        // The test rule has started Chromium, but this hasn't finished startup, like when WebView
        // startup is deferred.
        StartupController startupController = new StartupController(/* delegate= */ null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwWebContent webContent = new AwWebContent(startupController);
                    List<AwContents> boundAwContents = new ArrayList<>();
                    webContent.bindSurface(boundAwContents::add);

                    // A WebView that picked up webContent queues adopting it until startup
                    // finishes (see WebViewChromium#init), and then WebContent#close() is called.
                    startupController.getRunQueue().addTask(() -> adopt(webContent));
                    webContent.destroy();

                    startupController.getRunQueue().notifyChromiumStarted();

                    // As without deferred startup, webContent is adopted and then destroyed.
                    AwContents awContents = webContent.getAwContents();
                    assertNotNull(awContents);
                    assertEquals(Arrays.asList(null, awContents, null), boundAwContents);
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContextWrapperCannotBeReused() {
        StartupController startupController = new StartupController(/* delegate= */ null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwWebContent webContent = new AwWebContent(startupController);
                    WebContentContextWrapper wrapper =
                            new WebContentContextWrapper(
                                    mActivityTestRule.getActivity(), webContent);

                    assertSame(webContent, WebContentContextWrapper.getWebContent(wrapper));
                    assertNull(WebContentContextWrapper.getWebContent(wrapper));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_HYPERLINK_CONTEXT_MENU})
    public void testCreateDestroyedAwWebContent() {
        StartupController startupController = new StartupController(/* delegate= */ null);
        WeakReference<Context> contextRef =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AwWebContent destroyed =
                                    AwWebContent.createDestroyedAwWebContent(
                                            startupController,
                                            mActivityTestRule.getAwBrowserContext(),
                                            awContents -> new TestAwContentsClient());
                            AwContents awContents = destroyed.getAwContents();
                            assertNotNull(awContents);
                            Context context = awContents.getProvidedContext();
                            assertNull(
                                    "The destroyed AwWebContent is cached so it must not hold"
                                            + " activity references.",
                                    ContextUtils.activityFromContext(context));
                            assertSame(
                                    "Context must be wrapped for WebView resource lookup",
                                    context,
                                    ClassLoaderContextWrapperFactory.get(context));
                            return new WeakReference<>(context);
                        });

        CriteriaHelper.pollUiThread(
                () -> GarbageCollectionTestUtils.canBeGarbageCollected(contextRef),
                "Destroyed AwWebContent context should be garbage collected.");
    }

    private void adopt(AwWebContent webContent) {
        AwTestContainerView containerView =
                new AwTestContainerView(mActivityTestRule.getActivity(), false);
        webContent.adopt(
                new AwWebContent.ViewHost() {
                    @Override
                    public void initSettings(AwSettings settings) {}

                    @Override
                    public void onDetached() {}
                },
                mActivityTestRule.getAwBrowserContext(),
                containerView,
                containerView.getContext(),
                containerView.getInternalAccessDelegate(),
                (canvas, functor) -> {},
                awContents -> new TestAwContentsClient(),
                new AwContents.DependencyFactory());
    }
}
