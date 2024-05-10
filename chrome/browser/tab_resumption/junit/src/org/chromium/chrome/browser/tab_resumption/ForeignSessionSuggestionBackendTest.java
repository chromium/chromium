// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;

import java.util.List;

/** Unit tests for ForeignSessionSuggestionBackend. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ForeignSessionSuggestionBackendTest extends TestSupport {
    @Mock private ForeignSessionHelper mForeignSessionHelper;

    private ForeignSessionSuggestionBackend mSuggestionBackend;

    private boolean mIsCalled;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mSuggestionBackend =
                new ForeignSessionSuggestionBackend(mForeignSessionHelper) {
                    @Override
                    long getCurrentTimeMs() {
                        return CURRENT_TIME_MS;
                    }
                };
    }

    @After
    public void tearDown() {
        mSuggestionBackend.destroy();
    }

    @Test
    @SmallTest
    public void testReadCached() {
        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsA());
        mIsCalled = false;
        mSuggestionBackend.readCached(
                (List<SuggestionEntry> suggestions) -> {
                    assertSuggestionsEqual(makeForeignSessionSuggestionsA(), suggestions);
                    mIsCalled = true;
                });
        assert mIsCalled;

        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsB());
        mIsCalled = false;
        mSuggestionBackend.readCached(
                (List<SuggestionEntry> suggestions) -> {
                    assertSuggestionsEqual(makeForeignSessionSuggestionsB(), suggestions);
                    mIsCalled = true;
                });
        assert mIsCalled;
    }
}
