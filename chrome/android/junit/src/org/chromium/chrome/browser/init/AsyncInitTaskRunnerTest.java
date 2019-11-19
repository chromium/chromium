// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.test.ShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link AsyncInitTaskRunner}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowAsyncTask.class})
public class AsyncInitTaskRunnerTest {
    private static final int THREAD_WAIT_TIME_MS = 1000;

    private LibraryLoader mLoader;
    private AsyncInitTaskRunner mRunner;
    private CountDownLatch mLatch;

    private VariationsSeedFetcher mVariationsSeedFetcher;

    public AsyncInitTaskRunnerTest() {
        mLoader = spy(LibraryLoader.getInstance());
        doNothing().when(mLoader).ensureInitialized(anyInt());
        LibraryLoader.setLibraryLoaderForTesting(mLoader);
        mVariationsSeedFetcher = mock(VariationsSeedFetcher.class);
        VariationsSeedFetcher.setVariationsSeedFetcherForTesting(mVariationsSeedFetcher);
        PostTask.setPrenativeThreadPoolExecutorForTesting(new RoboExecutorService());

        mLatch = new CountDownLatch(1);
        mRunner = spy(new AsyncInitTaskRunner() {
            @Override
            protected void onSuccess() {
                mLatch.countDown();
            }
            @Override
            protected void onFailure() {
                mLatch.countDown();
            }
            @Override
            protected Executor getTaskPerThreadExecutor() {
                return new RoboExecutorService();
            }
        });
        // Allow test to run on all builds
        when(mRunner.shouldFetchVariationsSeedDuringFirstRun()).thenReturn(true);
        doNothing().when(mRunner).prefetchLibrary();
    }

    @After
    public void tearDown() {
        PostTask.resetPrenativeThreadPoolExecutorForTesting();
    }

    @Test
    public void libraryLoaderOnlyTest() throws InterruptedException {
        mRunner.startBackgroundTasks(false, false);

        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mLoader).ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        verify(mRunner).onSuccess();
        verify(mVariationsSeedFetcher, never()).fetchSeed(anyString(), anyString(), anyString());
    }

    @Test
    public void libraryLoaderFailTest() throws InterruptedException {
        doThrow(new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED))
                .when(mLoader)
                .ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        mRunner.startBackgroundTasks(false, false);

        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mRunner).onFailure();
        verify(mVariationsSeedFetcher, never()).fetchSeed(anyString(), anyString(), anyString());
    }

    @Test
    public void fetchVariationsTest() throws InterruptedException {
        mRunner.startBackgroundTasks(false, true);

        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        assertTrue(mLatch.await(0, TimeUnit.SECONDS));
        verify(mLoader).ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        verify(mRunner).onSuccess();
        verify(mVariationsSeedFetcher).fetchSeed(anyString(), anyString(), anyString());
    }

    // TODO(aberent) Test for allocateChildConnection. Needs refactoring of ChildProcessLauncher to
    // make it mockable.
}
