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
import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.build.BuildConfig;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link SplitPreloader}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.O)
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

    private static class OnCompleteTracker implements SplitPreloader.OnComplete {
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
        BuildConfig.IS_BUNDLE = true;
        mContext = new MainContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);
        mPreloader = new SplitPreloader(mContext);
    }

    @After
    public void tearDown() {
        BuildConfig.IS_BUNDLE = false;
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

        OnCompleteTracker tracker = new OnCompleteTracker();
        mPreloader.preload(SPLIT_A, tracker);
        mPreloader.wait(SPLIT_A);

        assertThat(mContext.getUiThreadContextNames()).containsExactly(SPLIT_A);
        assertThat(mContext.getBackgroundThreadContextNames()).containsExactly(SPLIT_A);
        assertTrue(tracker.getUiContext().wasCreatedOnUiThread());
        assertEquals(tracker.getUiContext().getName(), SPLIT_A);
        assertFalse(tracker.getBackgroundContext().wasCreatedOnUiThread());
        assertEquals(tracker.getBackgroundContext().getName(), SPLIT_A);
    }

    @Test
    public void testPreload_multipleWaitCalls() {
        initSplits(SPLIT_A);

        OnCompleteTracker tracker = new OnCompleteTracker();
        mPreloader.preload(SPLIT_A, tracker);
        mPreloader.wait(SPLIT_A);
        mPreloader.wait(SPLIT_A);
        mPreloader.wait(SPLIT_A);

        assertThat(mContext.getUiThreadContextNames()).containsExactly(SPLIT_A);
        assertThat(mContext.getBackgroundThreadContextNames()).containsExactly(SPLIT_A);
        assertTrue(tracker.getUiContext().wasCreatedOnUiThread());
        assertEquals(tracker.getUiContext().getName(), SPLIT_A);
    }

    @Test
    public void testPreload_withOnComplete_multipleSplitsInstalled() {
        initSplits(SPLIT_A, SPLIT_B);

        OnCompleteTracker trackerA = new OnCompleteTracker();
        mPreloader.preload(SPLIT_A, trackerA);

        OnCompleteTracker trackerB = new OnCompleteTracker();
        mPreloader.preload(SPLIT_B, trackerB);
        mPreloader.wait(SPLIT_A);
        mPreloader.wait(SPLIT_B);

        assertThat(mContext.getUiThreadContextNames()).containsExactly(SPLIT_A, SPLIT_B);
        assertThat(mContext.getBackgroundThreadContextNames()).containsExactly(SPLIT_A, SPLIT_B);
        assertTrue(trackerA.getUiContext().wasCreatedOnUiThread());
        assertEquals(trackerA.getUiContext().getName(), SPLIT_A);

        assertTrue(trackerB.getUiContext().wasCreatedOnUiThread());
        assertEquals(trackerB.getUiContext().getName(), SPLIT_B);
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
        Context[] backgroundContextHolder = new Context[1];
        Context[] uiContextHolder = new Context[1];
        CallbackHelper helper = new CallbackHelper();
        mPreloader.preload(
                SPLIT_A,
                new SplitPreloader.OnComplete() {
                    @Override
                    public void runImmediatelyInBackgroundThread(Context context) {
                        backgroundContextHolder[0] = context;
                        helper.notifyCalled();
                    }

                    @Override
                    public void runInUiThread(Context context) {
                        uiContextHolder[0] = context;
                    }
                });
        helper.waitForOnly();
        assertEquals(backgroundContextHolder[0], mContext);

        mPreloader.wait(SPLIT_A);

        assertThat(mContext.getUiThreadContextNames()).isEmpty();
        assertThat(mContext.getBackgroundThreadContextNames()).isEmpty();
        assertEquals(uiContextHolder[0], mContext);
    }
}
