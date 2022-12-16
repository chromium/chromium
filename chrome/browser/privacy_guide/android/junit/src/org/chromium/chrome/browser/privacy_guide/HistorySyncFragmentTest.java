// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import androidx.appcompat.widget.SwitchCompat;
import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.HashSet;
import java.util.Set;

/**
 * JUnit tests of the class {@link HistorySyncFragment}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class HistorySyncFragmentTest {
    private static final String CHANGE_HISTORY_SYNC_ON_USER_ACTION =
            "Settings.PrivacyGuide.ChangeHistorySyncOn";
    private static final String CHANGE_HISTORY_SYNC_OFF_USER_ACTION =
            "Settings.PrivacyGuide.ChangeHistorySyncOff";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private SyncService mSyncService;

    @Captor
    private ArgumentCaptor<Set<Integer>> mSetCaptor;

    private FragmentScenario mScenario;
    private SwitchCompat mHistorySyncButton;
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        SyncService.overrideForTests(mSyncService);
    }

    @After
    public void tearDown() {
        SyncService.resetForTests();
        if (mScenario != null) {
            mScenario.close();
        }
        mActionTester.tearDown();
    }

    private void initFragmentWithSyncState(boolean syncAll, boolean historySync) {
        initSyncState(syncAll, historySync);
        mScenario = FragmentScenario.launchInContainer(
                HistorySyncFragment.class, Bundle.EMPTY, R.style.Theme_MaterialComponents);
        mScenario.onFragment(fragment
                -> mHistorySyncButton = fragment.getView().findViewById(R.id.history_sync_switch));
    }

    private void initSyncState(boolean syncAll, boolean historySync) {
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(syncAll);

        if (syncAll) {
            historySync = true;
        }

        Set<Integer> syncTypes = new HashSet<>();
        if (historySync) {
            syncTypes.add(UserSelectableType.HISTORY);
        }
        when(mSyncService.getSelectedTypes()).thenReturn(syncTypes);
    }

    @Test
    public void testIsSwitchOffWhenHistorySyncOff() {
        initFragmentWithSyncState(false, false);
        assertFalse(mHistorySyncButton.isChecked());
    }

    @Test
    public void testIsSwitchOnWhenHistorySyncOnSyncAllOff() {
        initFragmentWithSyncState(false, true);
        assertTrue(mHistorySyncButton.isChecked());
    }

    @Test
    public void testIsSwitchOnWhenHistorySyncOnSyncAllOn() {
        initFragmentWithSyncState(true, true);
        assertTrue(mHistorySyncButton.isChecked());
    }

    @Test
    public void testTurnHistorySyncOffWhenSyncAllOn() {
        initFragmentWithSyncState(true, true);
        mHistorySyncButton.performClick();
        verify(mSyncService).setSelectedTypes(eq(false), mSetCaptor.capture());
        assertFalse(mSetCaptor.getValue().contains(UserSelectableType.HISTORY));
    }

    @Test
    public void testTurnHistorySyncOn() {
        initFragmentWithSyncState(false, false);
        mHistorySyncButton.performClick();
        verify(mSyncService).setSelectedTypes(eq(false), mSetCaptor.capture());
        assertTrue(mSetCaptor.getValue().contains(UserSelectableType.HISTORY));
    }

    @Test
    public void testTurnHistorySyncOffThenOnWhenSyncAllOn() {
        initFragmentWithSyncState(true, true);
        mHistorySyncButton.performClick();
        mHistorySyncButton.performClick();
        verify(mSyncService).setSelectedTypes(eq(true), mSetCaptor.capture());
        assertTrue(mSetCaptor.getValue().contains(UserSelectableType.HISTORY));
    }

    @Test
    public void testTurnHistorySyncOffThenOnWhenSyncAllOff() {
        initFragmentWithSyncState(false, true);
        mHistorySyncButton.performClick();
        mHistorySyncButton.performClick();
        verify(mSyncService, times(2)).setSelectedTypes(eq(false), mSetCaptor.capture());
        assertTrue(mSetCaptor.getAllValues().get(1).contains(UserSelectableType.HISTORY));
    }

    @Test
    public void testTurnHistorySyncOn_changeHistorySyncOnUserAction() {
        initFragmentWithSyncState(false, false);
        mHistorySyncButton.performClick();
        assertTrue(mActionTester.getActions().contains(CHANGE_HISTORY_SYNC_ON_USER_ACTION));
    }

    @Test
    public void testTurnHistorySyncOffWhenSyncAllOn_changeHistorySyncOffUserAction() {
        initFragmentWithSyncState(true, true);
        mHistorySyncButton.performClick();
        assertTrue(mActionTester.getActions().contains(CHANGE_HISTORY_SYNC_OFF_USER_ACTION));
    }

    @Test
    public void testTurnHistorySyncOffWhenSyncAllOff_changeHistorySyncOffUserAction() {
        initFragmentWithSyncState(false, true);
        mHistorySyncButton.performClick();
        assertTrue(mActionTester.getActions().contains(CHANGE_HISTORY_SYNC_OFF_USER_ACTION));
    }

    @Test
    public void testTurnHistorySyncOffThenOnWhenSyncAllOn_changeHistorySyncOffOnUserAction() {
        initFragmentWithSyncState(true, true);
        mHistorySyncButton.performClick();
        assertTrue(mActionTester.getActions().contains(CHANGE_HISTORY_SYNC_OFF_USER_ACTION));
        mHistorySyncButton.performClick();
        assertTrue(mActionTester.getActions().contains(CHANGE_HISTORY_SYNC_ON_USER_ACTION));
    }

    @Test
    public void testTurnHistorySyncOffThenOnWhenSyncAllOff_changeHistorySyncOffOnUserAction() {
        initFragmentWithSyncState(false, true);
        mHistorySyncButton.performClick();
        assertTrue(mActionTester.getActions().contains(CHANGE_HISTORY_SYNC_OFF_USER_ACTION));
        mHistorySyncButton.performClick();
        assertTrue(mActionTester.getActions().contains(CHANGE_HISTORY_SYNC_ON_USER_ACTION));
    }
}
