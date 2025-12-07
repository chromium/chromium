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
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Unit tests for {@link PendingRunnable}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

        ShadowLooper.idleMainLooper();
        Mockito.verify(mRunnable, times(1)).run();

        ShadowLooper.idleMainLooper();
        Mockito.verify(mRunnable, times(1)).run();
    }
}
