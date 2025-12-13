// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Holder;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link SplitPreloader}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SplitPreloaderTest {
    private static final String SPLIT_A = "split_a";
    private static final String SPLIT_B = "split_b";

    private static class SplitContext extends ContextWrapper {
        private final String mName;
        private final boolean mCreatedOnUiThread;

        public SplitContext(Context context, String name) {
            super(context);
            mName = name;
            mCreatedOnUiThread = ThreadUtils.runningOnUiThread();
        }

        public String getName() {
            return mName;
        }

        public boolean wasCreatedOnUiThread() {
            return mCreatedOnUiThread;
        }
    }

    private static class MainContext extends ContextWrapper {
        private final List<String> mUiThreadContextNames = new ArrayList<>();
        private final List<String> mBackgroundThreadContextNames = new ArrayList<>();

        public MainContext(Context context) {
            super(context);
        }

        @Override
        public Context createContextForSplit(String name)
                throws PackageManager.NameNotFoundException {
            if (ThreadUtils.runningOnUiThread()) {
                mUiThreadContextNames.add(name);
            } else {
                synchronized (mBackgroundThreadContextNames) {
                    mBackgroundThreadContextNames.add(name);
                }
            }
            return new SplitContext(this, name);
        }

        public List<String> getUiThreadContextNames() {
            return mUiThreadContextNames;
        }

        public List<String> getBackgroundThreadContextNames() {
            synchronized (mBackgroundThreadContextNames) {
                return new ArrayList<>(mBackgroundThreadContextNames);
            }
        }
    }

    private static class PreloadHooksTracker implements SplitPreloader.PreloadHooks {
        private SplitContext mBackgroundContext;
        private SplitContext mUiContext;

        @Override
        public void runImmediatelyInBackgroundThread(Context context) {
            assertNull(mBackgroundContext);
            mBackgroundContext = (SplitContext) context;
        }

        @Override
        public void runInUiThread(Context context) {
            assertNull(mUiContext);
            mUiContext = (SplitContext) context;
        }

        @Override
        public Context createIsolatedSplitContext(String name) {
            return BundleUtils.createIsolatedSplitContext(name);
        }

        public SplitContext getBackgroundContext() {
            return mBackgroundContext;
        }

        public SplitContext getUiContext() {
            return mUiContext;
        }
    }

    private MainContext mContext;
    private SplitPreloader mPreloader;

    @Before
    public void setUp() {
        BaseRobolectricTestRule.uninstallPausedExecutorService();
        BundleUtils.setHasSplitsForTesting(true);
        mContext = new MainContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);
        mPreloader = new SplitPreloader(mContext);
    }

    private void initSplits(String... names) {
        mContext.getApplicationInfo().splitNames = names;
        mContext.getApplicationInfo().splitSourceDirs = names;
    }

    @Test
    public void testPreload_splitInstalled() {
        initSplits(SPLIT_A);

        mPreloader.preload(SPLIT_A, null);
        mPreloader.wait(SPLIT_A);

        assertThat(mContext.getUiThreadContextNames()).isEmpty();
        assertThat(mContext.getBackgroundThreadContextNames()).containsExactly(SPLIT_A);
    }

    @Test
    public void testPreload_withOnComplete_splitInstalled() {
        initSplits(SPLIT_A);

        PreloadHooksTracker tracker = new PreloadHooksTracker();
        mPreloader.preload(SPLIT_A, tracker);
        mPreloader.wait(SPLIT_A);

        assertThat(mContext.getUiThreadContextNames()).containsExactly(SPLIT_A);
        assertThat(mContext.getBackgroundThreadContextNames()).containsExactly(SPLIT_A);
        assertTrue(tracker.getUiContext().wasCreatedOnUiThread());
        assertEquals(SPLIT_A, tracker.getUiContext().getName());
        assertFalse(tracker.getBackgroundContext().wasCreatedOnUiThread());
        assertEquals(SPLIT_A, tracker.getBackgroundContext().getName());
    }

    @Test
    public void testPreload_multipleWaitCalls() {
        initSplits(SPLIT_A);

        PreloadHooksTracker tracker = new PreloadHooksTracker();
        mPreloader.preload(SPLIT_A, tracker);
        mPreloader.wait(SPLIT_A);
        mPreloader.wait(SPLIT_A);
        mPreloader.wait(SPLIT_A);

        assertThat(mContext.getUiThreadContextNames()).containsExactly(SPLIT_A);
        assertThat(mContext.getBackgroundThreadContextNames()).containsExactly(SPLIT_A);
        assertTrue(tracker.getUiContext().wasCreatedOnUiThread());
        assertEquals(SPLIT_A, tracker.getUiContext().getName());
    }

    @Test
    public void testPreload_withOnComplete_multipleSplitsInstalled() {
        initSplits(SPLIT_A, SPLIT_B);

        PreloadHooksTracker trackerA = new PreloadHooksTracker();
        mPreloader.preload(SPLIT_A, trackerA);

        PreloadHooksTracker trackerB = new PreloadHooksTracker();
        mPreloader.preload(SPLIT_B, trackerB);
        mPreloader.wait(SPLIT_A);
        mPreloader.wait(SPLIT_B);

        assertThat(mContext.getUiThreadContextNames()).containsExactly(SPLIT_A, SPLIT_B);
        assertThat(mContext.getBackgroundThreadContextNames()).containsExactly(SPLIT_A, SPLIT_B);
        assertTrue(trackerA.getUiContext().wasCreatedOnUiThread());
        assertEquals(SPLIT_A, trackerA.getUiContext().getName());

        assertTrue(trackerB.getUiContext().wasCreatedOnUiThread());
        assertEquals(SPLIT_B, trackerB.getUiContext().getName());
    }

    @Test
    public void testPreload_splitNotInstalled() {
        mPreloader.preload(SPLIT_A, null);
        mPreloader.wait(SPLIT_A);

        assertThat(mContext.getUiThreadContextNames()).isEmpty();
        assertThat(mContext.getBackgroundThreadContextNames()).isEmpty();
    }

    @Test
    public void testPreload_withOnComplete_splitNotInstalled() throws Exception {
        Holder<@Nullable Context> backgroundContextHolder = new Holder<>(null);
        Holder<@Nullable Context> uiContextHolder = new Holder<>(null);
        CallbackHelper helper = new CallbackHelper();
        mPreloader.preload(
                SPLIT_A,
                new SplitPreloader.PreloadHooks() {
                    @Override
                    public void runImmediatelyInBackgroundThread(Context context) {
                        backgroundContextHolder.value = context;
                        helper.notifyCalled();
                    }

                    @Override
                    public void runInUiThread(Context context) {
                        uiContextHolder.value = context;
                    }

                    @Override
                    public Context createIsolatedSplitContext(String name) {
                        return BundleUtils.createIsolatedSplitContext(name);
                    }
                });
        helper.waitForOnly();
        assertEquals(backgroundContextHolder.value, mContext);

        mPreloader.wait(SPLIT_A);

        assertThat(mContext.getUiThreadContextNames()).isEmpty();
        assertThat(mContext.getBackgroundThreadContextNames()).isEmpty();
        assertEquals(uiContextHolder.value, mContext);
    }
}
