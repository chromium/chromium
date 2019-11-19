// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.testing.conformance.storage.ContentStorageConformanceTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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
public final class FeedContentStorageConformanceTest extends ContentStorageConformanceTest {
    private static final long TIMEOUT = scaleTimeout(3000);

    class ContentStorageWrapper extends FeedContentStorage {
        public ContentStorageWrapper(Profile p) {
            super(p);
        }

        @Override
        public void getAllKeys(Consumer < Result < List<String>>> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.getAllKeys(wrapper); }, TIMEOUT);
        }

        @Override
        public void get(List<String> keys, Consumer < Result < Map<String, byte[]>>> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.get(keys, wrapper); }, TIMEOUT);
        }

        @Override
        public void getAll(String prefix, Consumer < Result < Map<String, byte[]>>> consumer) {
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
            storage = new ContentStorageWrapper(Profile.getLastUsedProfile());
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ((ContentStorageWrapper) storage).destroy(); });
        storage = null;
    }
}
