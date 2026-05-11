// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.util.RunnableTimer;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.SYNC_TAB_SCREENSHOTS)
public class TabScreenshotSyncHelperUnitTest {
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabContentManager mTabContentManager;
    @Mock private Tab mTab;
    @Mock private NavigationHandle mNavigationHandle;
    @Mock private RunnableTimer mTimer;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private TabScreenshotSyncHelper mHelper;

    @Before
    public void setUp() {
        when(mTab.getId()).thenReturn(42);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isHidden()).thenReturn(false);

        when(mNavigationHandle.hasCommitted()).thenReturn(true);
        when(mNavigationHandle.isErrorPage()).thenReturn(false);

        mHelper = new TabScreenshotSyncHelper(mTabModelSelector, mTabContentManager, mTimer);
    }

    @Test
    public void testNavigationTriggersScreenshot() {
        mHelper.onDidStartNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        mHelper.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);

        verify(mTimer, times(2)).cancelTimer();
        verify(mTimer).startTimer(eq(5000L), any(Runnable.class));
    }

    @Test
    public void testNavigationStartCancelsTimer() {
        mHelper.onDidStartNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        verify(mTimer).cancelTimer();
    }

    @Test
    public void testNavigationIgnoredIfNotCommitted() {
        when(mNavigationHandle.hasCommitted()).thenReturn(false);

        mHelper.onDidStartNavigationInPrimaryMainFrame(mTab, mNavigationHandle);
        mHelper.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);

        verify(mTimer, never()).startTimer(any(Long.class), any(Runnable.class));
    }

    @Test
    public void testDestroyCancelsTimer() {
        mHelper.destroy();
        verify(mTimer).cancelTimer();
    }
}
