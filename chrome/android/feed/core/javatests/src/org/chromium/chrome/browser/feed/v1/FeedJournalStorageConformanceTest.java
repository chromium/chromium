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
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.testing.conformance.storage.JournalStorageConformanceTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/**
 * Conformance Tests for {@link FeedJournalStorage}.
 * The actual tests are implemented in JournalConformanceTest.
 */

// The @SmallTest class annotation is needed to allow the inherited @Test methods to run using
// build/android/test_runner.py.
@SmallTest
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableFeatures({ChromeFeatureList.INTEREST_FEED_V2})
public final class FeedJournalStorageConformanceTest extends JournalStorageConformanceTest {
    private static final long TIMEOUT = scaleTimeout(3000);

    /**
     * JournalStorageWrapper executes FeedJournalStorage operations
     * in the UiThread and blocks the test thread until the consumer
     * has been called.
     */
    class JournalStorageWrapper extends FeedJournalStorage {
        public JournalStorageWrapper(Profile p) {
            super(p);
        }

        @Override
        public void read(String journalName, Consumer<Result<List<byte[]>>> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.read(journalName, wrapper); }, TIMEOUT);
        }

        @Override
        public void commit(JournalMutation mutation, Consumer<CommitResult> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.commit(mutation, wrapper); }, TIMEOUT);
        }

        @Override
        public void exists(String journalName, Consumer<Result<Boolean>> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.exists(journalName, wrapper); }, TIMEOUT);
        }

        @Override
        public void getAllJournals(Consumer<Result<List<String>>> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.getAllJournals(wrapper); }, TIMEOUT);
        }

        @Override
        public void deleteAll(Consumer<CommitResult> consumer) {
            ConsumerSyncWrapper.waitForConsumer(
                    consumer, (wrapper) -> { super.deleteAll(wrapper); }, TIMEOUT);
        }
    }

    @Rule
    public final ChromeBrowserTestRule mRule = new ChromeBrowserTestRule();

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // journalStorage is declared and tested in JournalStorageConformanceTest.
            mJournalStorage = new JournalStorageWrapper(Profile.getLastUsedRegularProfile());
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ((JournalStorageWrapper) mJournalStorage).destroy(); });
        mJournalStorage = null;
    }
}
