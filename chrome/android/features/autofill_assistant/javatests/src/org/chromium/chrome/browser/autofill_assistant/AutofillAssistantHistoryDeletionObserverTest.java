// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.history.HistoryDeletionInfo;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferencesUtil;

/** Tests for the autofill assistant history deletion observer. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AutofillAssistantHistoryDeletionObserverTest {
    @Mock
    HistoryDeletionInfo mHistoryDeletionInfo;

    AutofillAssistantHistoryDeletionObserver mHistoryDeletionObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHistoryDeletionObserver = new AutofillAssistantHistoryDeletionObserver();
    }

    @After
    public void tearDown() {
        // Clear set preferences.
        AutofillAssistantPreferencesUtil.onClearBrowserHistory();
    }

    @Test
    @SmallTest
    public void clearFirstTimeUserFlagOnAllTimeHistoryDeletion() {
        AutofillAssistantPreferencesUtil.setFirstTimeTriggerScriptUserPreference(false);
        when(mHistoryDeletionInfo.isTimeRangeForAllTime()).thenReturn(true);

        mHistoryDeletionObserver.onURLsDeleted(mHistoryDeletionInfo);
        Assert.assertTrue(
                AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeTriggerScriptUser());
    }

    @Test
    @SmallTest
    public void doesNotClearFirstTimeUserFlagOnPartialHistoryDeletion() {
        AutofillAssistantPreferencesUtil.setFirstTimeTriggerScriptUserPreference(false);
        when(mHistoryDeletionInfo.isTimeRangeForAllTime()).thenReturn(false);

        mHistoryDeletionObserver.onURLsDeleted(mHistoryDeletionInfo);
        Assert.assertFalse(
                AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeTriggerScriptUser());
    }
}
