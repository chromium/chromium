// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.ntp.RecentlyClosedTabManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Collections;

/** Unit tests for {@link RecentlyClosedEntriesManagerTrackerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
public class RecentlyClosedEntriesManagerTrackerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private RecentlyClosedTabManager mRecentlyClosedTabManager;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;

    private RecentlyClosedEntriesManagerTrackerImpl mTracker;

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        RecentlyClosedEntriesManager.setRecentlyClosedTabManagerForTests(mRecentlyClosedTabManager);
        mTracker =
                (RecentlyClosedEntriesManagerTrackerImpl)
                        RecentlyClosedEntriesManagerTrackerFactory.getInstance();
    }

    @Test
    public void testDestroy() {
        RecentlyClosedEntriesManager manager =
                mTracker.obtainManager(mMultiInstanceManager, mTabModelSelector);
        assertTrue(mTracker.getManagers().contains(manager));

        mTracker.destroy(manager);

        verify(mRecentlyClosedTabManager).destroy();
        assertFalse(mTracker.getManagers().contains(manager));
    }

    @Test
    public void testOnWindowClosed_NotifiesAllManagers() {
        // Create two managers.
        RecentlyClosedEntriesManager manager1 =
                mTracker.obtainManager(mMultiInstanceManager, mTabModelSelector);
        RecentlyClosedEntriesManager manager2 =
                mTracker.obtainManager(mMultiInstanceManager, mTabModelSelector);

        // Simulate window closure.
        InstanceInfo info =
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ -1,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 1,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 3);
        mTracker.onInstancesClosed(
                Collections.singletonList(info), /* isPermanentDeletion= */ false);

        assertEquals(1, manager1.getRecentlyClosedEntries().size());
        assertEquals(1, manager2.getRecentlyClosedEntries().size());
    }

    @Test
    public void testOnWindowRestored_NotifiesAllManagers() {
        // Create two managers.
        RecentlyClosedEntriesManager manager1 =
                mTracker.obtainManager(mMultiInstanceManager, mTabModelSelector);
        RecentlyClosedEntriesManager manager2 =
                mTracker.obtainManager(mMultiInstanceManager, mTabModelSelector);
        // Simulate window closure.
        InstanceInfo info =
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ -1,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 1,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 3);
        mTracker.onInstancesClosed(
                Collections.singletonList(info), /* isPermanentDeletion= */ false);
        assertEquals(1, manager1.getRecentlyClosedEntries().size());
        assertEquals(1, manager2.getRecentlyClosedEntries().size());

        // Simulate window restoration.
        mTracker.onInstanceRestored(/* instanceId= */ 2);

        assertEquals(0, manager1.getRecentlyClosedEntries().size());
        assertEquals(0, manager2.getRecentlyClosedEntries().size());
    }
}
