// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.runner.RunWith;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.testing.conformance.storage.ContentStorageConformanceTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;
import java.util.Map;

/**
 * Conformance Tests for {@link FeedContentStorage}.
 * The actual tests are implemented in ContentStorageConformanceTest.
 */

// The @SmallTest class annotation is needed to allow the inherited @Test methods to run using
// build/android/test_runner.py.
@SmallTest
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableFeatures({ChromeFeatureList.INTEREST_FEED_V2})
public final class FeedContentStorageConformanceTest extends ContentStorageConformanceTest {
    private static final long TIMEOUT = scaleTimeout(3000);

    class ContentStorageWrapper extends FeedContentStorage {
        public ContentStorageWrapper(Profile p) {
            super(p);
        }

        @Override
        public void getAllKeys(Consumer<Result<List<String>>> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.getAllKeys(wrapper); }, TIMEOUT);
        }

        @Override
        public void get(List<String> keys, Consumer<Result<Map<String, byte[]>>> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.get(keys, wrapper); }, TIMEOUT);
        }

        @Override
        public void getAll(String prefix, Consumer<Result<Map<String, byte[]>>> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.getAll(prefix, wrapper); }, TIMEOUT);
        }

        @Override
        public void commit(ContentMutation mutation, Consumer<CommitResult> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.commit(mutation, wrapper); }, TIMEOUT);
        }
    }

    @Rule
    public final ChromeBrowserTestRule mRule = new ChromeBrowserTestRule();

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // storage is declared and tested in ContentStorageConformanceTest.
            mStorage = new ContentStorageWrapper(Profile.getLastUsedRegularProfile());
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ((ContentStorageWrapper) mStorage).destroy(); });
        mStorage = null;
    }
}
