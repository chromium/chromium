// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

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
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.tab_resumption.ForeignSessionSuggestionBackend.UrlFilteringDelegate;

import java.util.List;

/** Unit tests for ForeignSessionSuggestionBackend. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ForeignSessionSuggestionBackendUnitTest extends TestSupport {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ForeignSessionHelper mForeignSessionHelper;
    @Mock private UrlFilteringDelegate mUrlFilteringDelegate;

    private ForeignSessionSuggestionBackend mSuggestionBackend;

    private boolean mIsCalled;

    @Before
    public void setUp() {
        TabResumptionModuleUtils.setFakeCurrentTimeMsForTesting(() -> CURRENT_TIME_MS);
        mSuggestionBackend =
                new ForeignSessionSuggestionBackend(mForeignSessionHelper, mUrlFilteringDelegate);
    }

    @After
    public void tearDown() {
        mSuggestionBackend.destroy();
        TabResumptionModuleUtils.setFakeCurrentTimeMsForTesting(null);
    }

    @Test
    @SmallTest
    public void testRead() {
        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsA());
        mIsCalled = false;
        mSuggestionBackend.read(
                (List<SuggestionEntry> suggestions) -> {
                    assertSuggestionsEqual(makeForeignSessionSuggestionsA(), suggestions);
                    mIsCalled = true;
                });
        assert mIsCalled;

        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsB());
        mIsCalled = false;
        mSuggestionBackend.read(
                (List<SuggestionEntry> suggestions) -> {
                    assertSuggestionsEqual(makeForeignSessionSuggestionsB(), suggestions);
                    mIsCalled = true;
                });
        assert mIsCalled;
    }

    @Test
    @SmallTest
    public void testUrlFiltering() {
        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsA());
        when(mUrlFilteringDelegate.shouldExcludeUrl(eq(TAB1.url))).thenReturn(true);
        List<SuggestionEntry> expectedSuggestions = makeForeignSessionSuggestionsA();
        // The index of TAB1 is 2.
        expectedSuggestions.remove(2);

        mIsCalled = false;
        mSuggestionBackend.read(
                (List<SuggestionEntry> suggestions) -> {
                    assertSuggestionsEqual(expectedSuggestions, suggestions);
                    mIsCalled = true;
                });
        assert mIsCalled;
    }
}
