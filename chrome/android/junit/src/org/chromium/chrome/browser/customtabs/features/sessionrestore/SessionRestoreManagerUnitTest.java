// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.os.Handler;
import android.os.Looper;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.customtabs.features.sessionrestore.SessionRestoreManagerImpl.EvictionReason;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.TimeUnit;

/**
 * Unit test related to {@link SessionRestoreManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLooper.class, ShadowPostTask.class})
@LooperMode(Mode.PAUSED)
public class SessionRestoreManagerUnitTest {
    private static final int TEST_EVICTION_TIMEOUT = 10;

    @Rule
    public CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();
    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    TabFreezer mTabFreezer;
    @Mock
    TabInteractionRecorder mTabInteractionRecorder;

    private SessionRestoreManager mSessionRestoreManager;
    private @EvictionReason PayloadCallbackHelper<Integer> mEvictionCallback;
    private Tab mTab;

    @Before
    public void setup() {
        ChromeFeatureList.sCctRetainableStateInMemory.setForTesting(true);
        ShadowPostTask.setTestImpl(new ShadowPostTask.TestImpl() {
            final Handler mHandler = new Handler(Looper.getMainLooper());

            @Override
            public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
                mHandler.postDelayed(task, delay);
            }
        });
        mTab = env.prepareTab();
        mEvictionCallback = new PayloadCallbackHelper<>();
        TabInteractionRecorder.setInstanceForTesting(mTabInteractionRecorder);
        mSessionRestoreManager = new SessionRestoreManagerImpl(mTabFreezer);
        mSessionRestoreManager.addObserver(mEvictionCallback::notifyCalled);
        mSessionRestoreManager.setEvictionTimeout(TEST_EVICTION_TIMEOUT);
        doReturn(true).when(mTabFreezer).freeze(any());
    }

    @After
    public void tearDown() {
        TabInteractionRecorder.setInstanceForTesting(null);
        ShadowPostTask.setTestImpl(null);
    }

    @Test
    public void getSessionManagerWithFeature() {
        Mockito.doCallRealMethod().when(env.connection).getSessionRestoreManager();
        Assert.assertNotNull(
                "SessionRestoreManager is null.", env.connection.getSessionRestoreManager());
    }

    @Test
    public void nullSessionManagerWithoutFeature() {
        ChromeFeatureList.sCctRetainableStateInMemory.setForTesting(false);
        Mockito.doCallRealMethod().when(env.connection).getSessionRestoreManager();
        Assert.assertNull(
                "SessionRestoreManager should be null.", env.connection.getSessionRestoreManager());
    }

    @Test
    public void storeThenEvictedByTimeout() {
        assertTrue("Tab store failed.", mSessionRestoreManager.store(mTab));
        assertNotNull("ReparentingTask is not created for stored tab.",
                mTab.getUserDataHost().getUserData(ReparentingTask.class));
        verify(mTabInteractionRecorder).onTabClosing();

        ShadowSystemClock.advanceBy(TEST_EVICTION_TIMEOUT, TimeUnit.MILLISECONDS);
        Shadows.shadowOf(Looper.getMainLooper()).runOneTask();

        assertEvictedWithReason(EvictionReason.TIMEOUT);
    }

    @Test
    public void callClearCache() {
        doReturn(true).when(mTabFreezer).hasTab();
        mSessionRestoreManager.clearCache();

        assertEvictedWithReason(EvictionReason.CLIENT_CLEAN);
    }

    @Test
    public void restoreTab() {
        doReturn(true).when(mTabFreezer).hasTab();
        doReturn(mTab).when(mTabFreezer).unfreeze();

        assertEquals("Restoring tab failed.", mTab, mSessionRestoreManager.restoreTab());
        verify(mTabInteractionRecorder).reset();
        verify(mTabFreezer).unfreeze();
    }

    @Test
    public void storeNewTabOverridePreviousTab() {
        // This is a temporary test where we only support storing one tab.
        // TODO(https://crbug.com/1379452): Remove when support freezing multiple tabs.
        doReturn(true).when(mTabFreezer).hasTab();
        assertTrue("Tab store failed.", mSessionRestoreManager.store(mTab));
        assertNotNull("ReparentingTask is not created for stored tab.",
                mTab.getUserDataHost().getUserData(ReparentingTask.class));
        verify(mTabInteractionRecorder).onTabClosing();
        assertEvictedWithReason(EvictionReason.NEW_TAB_OVERRIDE);
    }

    private void assertEvictedWithReason(@EvictionReason int reason) {
        verify(mTabFreezer).clear();
        assertTrue("evictionCallback should be invoked.", mEvictionCallback.getCallCount() > 0);
        assertEquals("Eviction reason is different.", reason,
                (int) mEvictionCallback.getOnlyPayloadBlocking());
    }
}
