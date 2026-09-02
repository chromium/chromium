// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

/** Unit tests for {@link PendingRunnable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PendingRunnableTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mRunnable;

    @Test
    public void testPost() {
        PendingRunnable pendingRunnable = new PendingRunnable(TaskTraits.UI_DEFAULT, mRunnable);
        Mockito.verify(mRunnable, never()).run();

        pendingRunnable.post();
        Mockito.verify(mRunnable, never()).run();

        pendingRunnable.post();
        Mockito.verify(mRunnable, never()).run();

        RobolectricUtil.runAllBackgroundAndUi();
        Mockito.verify(mRunnable, times(1)).run();

        RobolectricUtil.runAllBackgroundAndUi();
        Mockito.verify(mRunnable, times(1)).run();
    }
}
