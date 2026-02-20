// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/** Tests for {@link AsyncInitTaskRunner} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AsyncInitTaskRunnerTest {
    private final LibraryLoader mLoader;
    private final AsyncInitTaskRunner mRunner;
    private final CountDownLatch mLatch;
    private final VariationsSeedFetcher mVariationsSeedFetcher;

    public AsyncInitTaskRunnerTest() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        mLoader = spy(LibraryLoader.getInstance());
        LibraryLoader.setLibraryLoaderForTesting(mLoader);
        mVariationsSeedFetcher = mock(VariationsSeedFetcher.class);
        VariationsSeedFetcher.setVariationsSeedFetcherForTesting(mVariationsSeedFetcher);

        mLatch = new CountDownLatch(1);
        mRunner =
                spy(
                        new AsyncInitTaskRunner() {
                            @Override
                            protected void onSuccess() {
                                mLatch.countDown();
                            }

                            @Override
                            protected void onFailure(Exception failureCause) {
                                mLatch.countDown();
                            }

                            @Override
                            protected Executor getTaskPerThreadExecutor() {
                                return PostTask.getBackgroundUserVisibleExecutor();
                            }
                        });
        // Allow test to run on all builds
        when(mRunner.shouldFetchVariationsSeedDuringFirstRun()).thenReturn(true);
    }

    @Test
    public void libraryLoaderOnlyTest() throws InterruptedException {
        mRunner.startBackgroundTasks(false, false);

        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mLoader).ensureInitialized();
        verify(mRunner).onSuccess();
        verify(mVariationsSeedFetcher, never()).fetchSeed(anyString(), anyString(), anyString());
    }

    @Test
    public void libraryLoaderFailTest() throws InterruptedException {
        Exception failureCause = new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED);
        doThrow(failureCause).when(mLoader).ensureInitialized();
        mRunner.startBackgroundTasks(false, false);

        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mRunner).onFailure(failureCause);
        verify(mVariationsSeedFetcher, never()).fetchSeed(anyString(), anyString(), anyString());
    }

    @Test
    public void fetchVariationsTest() throws InterruptedException {
        mRunner.startBackgroundTasks(false, true);

        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mLoader).ensureInitialized();
        verify(mRunner).onSuccess();
        verify(mVariationsSeedFetcher).fetchSeed(anyString(), anyString(), anyString());
    }

    // TODO(aberent) Test for allocateChildConnection. Needs refactoring of ChildProcessLauncher to
    // make it mockable.
}
