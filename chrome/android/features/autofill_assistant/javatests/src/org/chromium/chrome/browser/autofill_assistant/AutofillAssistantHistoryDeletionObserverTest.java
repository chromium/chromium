// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.history.HistoryDeletionInfo;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Tests for the autofill assistant history deletion observer. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AutofillAssistantHistoryDeletionObserverTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private Profile mProfileMock;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private PrefService mPrefServiceMock;
    @Mock
    HistoryDeletionInfo mHistoryDeletionInfo;

    AutofillAssistantHistoryDeletionObserver mHistoryDeletionObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfileMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        mHistoryDeletionObserver = new AutofillAssistantHistoryDeletionObserver();
    }

    @Test
    @SmallTest
    public void clearFirstTimeUserFlagOnAllTimeHistoryDeletion() {
        when(mHistoryDeletionInfo.isTimeRangeForAllTime()).thenReturn(true);

        mHistoryDeletionObserver.onURLsDeleted(mHistoryDeletionInfo);
        verify(mPrefServiceMock, times(1))
                .clearPref(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_IS_FIRST_TIME_USER);
    }

    @Test
    @SmallTest
    public void doesNotClearFirstTimeUserFlagOnPartialHistoryDeletion() {
        when(mHistoryDeletionInfo.isTimeRangeForAllTime()).thenReturn(false);

        mHistoryDeletionObserver.onURLsDeleted(mHistoryDeletionInfo);
        verify(mPrefServiceMock, times(0))
                .clearPref(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_IS_FIRST_TIME_USER);
    }
}
