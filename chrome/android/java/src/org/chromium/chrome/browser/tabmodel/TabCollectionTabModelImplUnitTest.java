// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;

/** Unit tests for {@link TabCollectionTabModelImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCollectionTabModelImplUnitTest {
    private static final long NATIVE_PTR = 875943L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelJniBridge.Natives mTabModelJniBridgeJni;
    @Mock private Profile mProfile;
    @Mock private Profile mOtrProfile;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private TabCreator mIncognitoTabCreator;
    @Mock private TabModelObserver mTabModelObserver;

    private TabCollectionTabModelImpl mTabModel;

    @Before
    public void setUp() {
        // Required to use MockTab.
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfile.isIncognitoBranded()).thenReturn(false);
        when(mOtrProfile.isOffTheRecord()).thenReturn(true);
        when(mOtrProfile.isIncognitoBranded()).thenReturn(true);

        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridgeJni);
        when(mTabModelJniBridgeJni.init(
                        any(),
                        eq(mProfile),
                        eq(ActivityType.TABBED),
                        /* isArchivedTabModel= */ eq(false)))
                .thenReturn(NATIVE_PTR);

        mTabModel =
                new TabCollectionTabModelImpl(
                        mProfile,
                        ActivityType.TABBED,
                        /* isArchivedTabModel= */ false,
                        mRegularTabCreator,
                        mIncognitoTabCreator);
        mTabModel.addObserver(mTabModelObserver);
    }

    @After
    public void tearDown() {
        mTabModel.destroy();
        verify(mTabModelJniBridgeJni).destroy(eq(NATIVE_PTR), any());
    }

    @Test
    public void testGetTabCreator() {
        assertEquals(mRegularTabCreator, mTabModel.getTabCreator());
    }

    @Test
    public void testBroadcastSessionRestoreComplete() {
        mTabModel.completeInitialization();

        verify(mTabModelObserver).restoreCompleted();
        assertTrue(mTabModel.isInitializationComplete());

        mTabModel.broadcastSessionRestoreComplete();

        verify(mTabModelJniBridgeJni).broadcastSessionRestoreComplete(eq(NATIVE_PTR), any());
    }

    @Test
    public void testCompleteInitializationTwice() {
        mTabModel.completeInitialization();
        assertThrows(AssertionError.class, mTabModel::completeInitialization);
    }

    @Test
    public void testAddTabBasic() {
        @TabId int tabId = 789;
        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        assertEquals(tab, mTabModel.getTabById(tabId));
    }

    @Test
    public void testAddTabDuplicate() {
        @TabId int tabId = 789;
        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        assertThrows(
                AssertionError.class,
                () ->
                        mTabModel.addTab(
                                tab,
                                /* index= */ 1,
                                TabLaunchType.FROM_CHROME_UI,
                                TabCreationState.LIVE_IN_FOREGROUND));
    }

    @Test
    public void testAddTabWrongModel() {
        @TabId int tabId = 789;
        MockTab otrTab = MockTab.createAndInitialize(tabId, mOtrProfile);
        assertThrows(
                IllegalStateException.class,
                () ->
                        mTabModel.addTab(
                                otrTab,
                                /* index= */ 1,
                                TabLaunchType.FROM_CHROME_UI,
                                TabCreationState.LIVE_IN_FOREGROUND));
    }
}
