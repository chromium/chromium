// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tabmodel.HeadlessTabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/** Unit tests for {@link MultiInstanceHeadlessTabModelObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MultiInstanceHeadlessTabModelObserverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int WINDOW_ID = 2;

    @Mock private HeadlessTabModelSelectorImpl mTabModelSelector;
    @Mock private TabModel mNormalTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabWindowManager mTabWindowManager;

    private MultiInstanceHeadlessTabModelObserver mObserver;

    @Before
    public void setUp() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        ChromeMultiInstancePersistentStore.ensureInitialized();
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getCurrentTabModelSupplier())
                .thenReturn(ObservableSuppliers.createMonotonic(mNormalTabModel));

        mObserver = new MultiInstanceHeadlessTabModelObserver(mTabModelSelector, WINDOW_ID);
    }

    @After
    public void tearDown() {
        ChromeMultiInstancePersistentStore.resetForTesting();
    }

    @Test
    public void testTabsRemaining_doesNotShutdown() {
        when(mNormalTabModel.getCount()).thenReturn(1);
        when(mIncognitoTabModel.getCount()).thenReturn(0);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(WINDOW_ID);

        mObserver.tabRemoved(/* tab= */ null);

        Assert.assertTrue(ChromeMultiInstancePersistentStore.hasInstance(WINDOW_ID));
        Assert.assertEquals(1, ChromeMultiInstancePersistentStore.readNormalTabCount(WINDOW_ID));
        Assert.assertEquals(0, ChromeMultiInstancePersistentStore.readIncognitoTabCount(WINDOW_ID));
    }

    @Test
    public void testNoTabsRemaining_shutdownHeadless() {
        when(mNormalTabModel.getCount()).thenReturn(0);
        when(mIncognitoTabModel.getCount()).thenReturn(0);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(WINDOW_ID);
        Assert.assertTrue(ChromeMultiInstancePersistentStore.hasInstance(WINDOW_ID));

        mObserver.tabRemoved(/* tab= */ null);

        // Defer shutdown is posted to UI thread.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertFalse(ChromeMultiInstancePersistentStore.hasInstance(WINDOW_ID));
    }

    @Test
    public void testDidAddTab() {
        when(mNormalTabModel.getCount()).thenReturn(2);
        when(mIncognitoTabModel.getCount()).thenReturn(1);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(WINDOW_ID);

        mObserver.didAddTab(
                /* tab= */ null,
                /* type= */ 0,
                /* creationState= */ 0,
                /* markedForSelection= */ false);

        Assert.assertTrue(ChromeMultiInstancePersistentStore.hasInstance(WINDOW_ID));
        Assert.assertEquals(2, ChromeMultiInstancePersistentStore.readNormalTabCount(WINDOW_ID));
        Assert.assertEquals(1, ChromeMultiInstancePersistentStore.readIncognitoTabCount(WINDOW_ID));
    }

    @Test
    public void testOnFinishingTabClosure() {
        when(mNormalTabModel.getCount()).thenReturn(1);
        when(mIncognitoTabModel.getCount()).thenReturn(1);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(WINDOW_ID);

        mObserver.onFinishingTabClosure(/* tab= */ null, /* closingSource= */ 0);

        Assert.assertTrue(ChromeMultiInstancePersistentStore.hasInstance(WINDOW_ID));
        Assert.assertEquals(1, ChromeMultiInstancePersistentStore.readNormalTabCount(WINDOW_ID));
        Assert.assertEquals(1, ChromeMultiInstancePersistentStore.readIncognitoTabCount(WINDOW_ID));
    }

    @Test
    public void testTabStateNotInitialized() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(WINDOW_ID);

        mObserver.tabRemoved(/* tab= */ null);

        verify(mNormalTabModel, never()).getCount();
        verify(mIncognitoTabModel, never()).getCount();
        Assert.assertTrue(ChromeMultiInstancePersistentStore.hasInstance(WINDOW_ID));
    }
}
