// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link ActorTabStateHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorTabStateHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;

    @Before
    public void setUp() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.iterator()).thenReturn(Collections.singletonList(mTab).iterator());

        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(500);
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
        ActorKeyedServiceFactory.setForTesting(null);
    }

    @Test
    public void testDetachActiveBackgroundSessions_WithActiveTask_TransitionsTab() {
        List<BackgroundSession> sessions =
                ActorTabStateHelper.detachActiveBackgroundSessions(mTabModelSelector);

        assertEquals(1, sessions.size());
        assertEquals(mTab, sessions.get(0).getLastActiveTab());
        assertEquals(Integer.valueOf(500), sessions.get(0).getTaskId());

        verify(mTabRemover).removeTab(mTab, false);
    }

    @Test
    public void testDetachActiveBackgroundSessions_NoActiveTask_NoTransition() {
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(null);

        List<BackgroundSession> sessions =
                ActorTabStateHelper.detachActiveBackgroundSessions(mTabModelSelector);

        assertTrue(sessions.isEmpty());
        verify(mTabRemover, never()).removeTab(any(), eq(false));
    }

    @Test
    public void testDetachActiveBackgroundSessions_MultipleTabsSameTask_GroupedInSession() {
        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(101);
        when(mTabModel.iterator()).thenReturn(Arrays.asList(mTab, tab2).iterator());
        when(mActorKeyedService.getActiveTaskIdOnTab(101, false)).thenReturn(500);

        List<BackgroundSession> sessions =
                ActorTabStateHelper.detachActiveBackgroundSessions(mTabModelSelector);

        assertEquals(1, sessions.size());
        assertEquals(2, sessions.get(0).getTabs().size());
        assertEquals(mTab, sessions.get(0).getTabs().get(0));
        assertEquals(tab2, sessions.get(0).getTabs().get(1));
        assertEquals(tab2, sessions.get(0).getLastActiveTab());

        verify(mTabRemover, never()).removeTab(mTab, false);
        verify(mTabRemover).removeTab(tab2, false);
    }
}
