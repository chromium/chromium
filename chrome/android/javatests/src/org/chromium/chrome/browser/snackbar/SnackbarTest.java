// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link SnackbarManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SnackbarTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private SnackbarManager mManager;
    private SnackbarController mDefaultController = new SnackbarController() {
        @Override
        public void onDismissNoAction(Object actionData) {
        }

        @Override
        public void onAction(Object actionData) {
        }
    };

    private SnackbarController mDismissController = new SnackbarController() {
        @Override
        public void onDismissNoAction(Object actionData) {
            mDismissed = true;
        }

        @Override
        public void onAction(Object actionData) { }
    };

    private boolean mDismissed;

    @Before
    public void setUp() throws InterruptedException {
        SnackbarManager.setDurationForTesting(1000);
        mActivityTestRule.startMainActivityOnBlankPage();
        mManager = mActivityTestRule.getActivity().getSnackbarManager();
    }

    @Test
    @MediumTest
    @RetryOnFailure
    public void testStackQueuePersistentOrder() {
        final Snackbar stackbar = Snackbar.make("stack", mDefaultController,
                Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        final Snackbar queuebar = Snackbar.make("queue", mDefaultController,
                Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_TEST_SNACKBAR);
        final Snackbar persistent = Snackbar.make("persistent", mDefaultController,
                Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> { mManager.showSnackbar(stackbar); });
        CriteriaHelper.pollUiThread(new Criteria("First snackbar not shown") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == stackbar;
            }
        });
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.showSnackbar(queuebar);
            Assert.assertTrue("Snackbar not showing", mManager.isShowing());
            Assert.assertEquals("Snackbars on stack should not be cancelled by snackbars on queue",
                    stackbar, mManager.getCurrentSnackbarForTesting());
        });
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mManager.showSnackbar(persistent);
            Assert.assertTrue("Snackbar not showing", mManager.isShowing());
            Assert.assertEquals(
                    "Snackbars on stack should not be cancelled by persistent snackbars", stackbar,
                    mManager.getCurrentSnackbarForTesting());
        });
        CriteriaHelper.pollUiThread(new Criteria("Snackbar on queue not shown") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == queuebar;
            }
        });
        CriteriaHelper.pollUiThread(new Criteria("Snackbar on queue not timed out") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing()
                        && mManager.getCurrentSnackbarForTesting() == persistent;
            }
        });
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.onClick(null));
        CriteriaHelper.pollUiThread(new Criteria("Persistent snackbar did not get cleared") {
            @Override
            public boolean isSatisfied() {
                return !mManager.isShowing();
            }
        });
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testPersistentQueueStackOrder() {
        final Snackbar stackbar = Snackbar.make("stack", mDefaultController,
                Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        final Snackbar queuebar = Snackbar.make("queue", mDefaultController,
                Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_TEST_SNACKBAR);
        final Snackbar persistent = Snackbar.make("persistent", mDefaultController,
                Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(persistent));
        CriteriaHelper.pollUiThread(new Criteria("First snackbar not shown") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing()
                        && mManager.getCurrentSnackbarForTesting() == persistent;
            }
        });
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(queuebar));
        CriteriaHelper.pollUiThread(new Criteria(
                "Persistent snackbar was not cleared by queue snackbar") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == queuebar;
            }
        });
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(stackbar));
        CriteriaHelper.pollUiThread(
                new Criteria("Snackbar on queue was not cleared by snackbar stack.") {
                    @Override
                    public boolean isSatisfied() {
                        return mManager.isShowing()
                                && mManager.getCurrentSnackbarForTesting() == stackbar;
                    }
                });
        CriteriaHelper.pollUiThread(new Criteria("Snackbar did not time out") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing()
                        && mManager.getCurrentSnackbarForTesting() == persistent;
            }
        });
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.onClick(null));
        CriteriaHelper.pollUiThread(new Criteria("Persistent snackbar did not get cleared") {
            @Override
            public boolean isSatisfied() {
                return !mManager.isShowing();
            }
        });
    }

    @Test
    @SmallTest
    public void testDismissSnackbar() {
        final Snackbar snackbar = Snackbar.make("stack", mDismissController,
                Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
        mDismissed = false;
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(snackbar));
        CriteriaHelper.pollUiThread(
                new Criteria("Snackbar on queue was not cleared by snackbar stack.") {
                    @Override
                    public boolean isSatisfied() {
                        return mManager.isShowing()
                                && mManager.getCurrentSnackbarForTesting() == snackbar;
                    }
                });
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> mManager.dismissSnackbars(mDismissController));
        CriteriaHelper.pollUiThread(new Criteria("Snackbar did not time out") {
            @Override
            public boolean isSatisfied() {
                return !mManager.isShowing() && mDismissed;
            }
        });
    }

    @Test
    @SmallTest
    public void testPersistentSnackbar() throws InterruptedException {
        int timeout = 100;
        SnackbarManager.setDurationForTesting(timeout);
        final Snackbar snackbar = Snackbar.make("persistent", mDismissController,
                Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR);
        mDismissed = false;
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.showSnackbar(snackbar));
        CriteriaHelper.pollUiThread(new Criteria("Persistent Snackbar not shown.") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == snackbar;
            }
        });
        TimeUnit.MILLISECONDS.sleep(timeout);
        CriteriaHelper.pollUiThread(new Criteria("Persistent snackbar timed out.") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing() && !mDismissed;
            }
        });
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> mManager.onClick(null));
        CriteriaHelper.pollUiThread(new Criteria("Persistent snackbar not removed on action.") {
            @Override
            public boolean isSatisfied() {
                return !mManager.isShowing();
            }
        });
    }
}
