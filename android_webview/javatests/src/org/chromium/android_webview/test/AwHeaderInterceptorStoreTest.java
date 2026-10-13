// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.http_headers.AwHeaderInterceptorStore;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

import java.util.Collections;
import java.util.List;

/** Tests for {@link AwHeaderInterceptorStore}. */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AwHeaderInterceptorStoreTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwHeaderInterceptorStore mStore;

    @Before
    public void setUp() {
        mActivityTestRule.startBrowserProcess();
        mStore =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> AwBrowserContext.getDefault().getHeaderInterceptorStore());
        ThreadUtils.runOnUiThreadBlocking(() -> mStore.clearInterceptors());
    }

    @After
    public void tearDown() {
        if (mStore != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mStore.clearInterceptors());
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddInterceptor() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int id =
                            mStore.addInterceptor(
                                    request -> Collections.emptyMap(),
                                    List.of("https://example.com/*"));

                    Assert.assertEquals(1, id);
                    Assert.assertEquals(1, mStore.getInterceptorCountForTesting());
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddInterceptor_emptyUrlPatterns_throwsIllegalArgumentException() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<String> emptyPatterns = Collections.emptyList();
                    IllegalArgumentException exception =
                            Assert.assertThrows(
                                    IllegalArgumentException.class,
                                    () ->
                                            mStore.addInterceptor(
                                                    request -> Collections.emptyMap(),
                                                    emptyPatterns));
                    Assert.assertTrue(
                            exception
                                    .getMessage()
                                    .contains("Must provide at least one URL pattern string."));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddInterceptor_invalidUrlPattern_throwsIllegalArgumentException() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<String> invalidPatterns = List.of("/*");
                    IllegalArgumentException exception =
                            Assert.assertThrows(
                                    IllegalArgumentException.class,
                                    () ->
                                            mStore.addInterceptor(
                                                    request -> Collections.emptyMap(),
                                                    invalidPatterns));
                    Assert.assertTrue(
                            exception.getMessage().contains("Failed to parse URL pattern"));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRemoveInterceptor() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int id =
                            mStore.addInterceptor(
                                    request -> Collections.emptyMap(),
                                    List.of("https://example.com/*"));

                    mStore.removeInterceptor(id);

                    Assert.assertEquals(0, mStore.getInterceptorCountForTesting());
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testClearInterceptors() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStore.addInterceptor(
                            request -> Collections.emptyMap(), List.of("https://example.com/*"));

                    mStore.clearInterceptors();

                    Assert.assertEquals(0, mStore.getInterceptorCountForTesting());
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testThrowsIllegalStateExceptionWhenNativeStoreUnavailable() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHeaderInterceptorStore store = new AwHeaderInterceptorStore(() -> null);
                    List<String> patterns = List.of("https://*/*");
                    Assert.assertThrows(
                            IllegalStateException.class,
                            () ->
                                    store.addInterceptor(
                                            request -> Collections.emptyMap(), patterns));
                    Assert.assertThrows(
                            IllegalStateException.class, () -> store.removeInterceptor(1));
                    Assert.assertThrows(
                            IllegalStateException.class, () -> store.clearInterceptors());
                    Assert.assertThrows(
                            IllegalStateException.class,
                            () -> store.getInterceptorCountForTesting());
                });
    }
}
