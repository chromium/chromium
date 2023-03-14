// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import androidx.appcompat.widget.AppCompatEditText;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.directactions.DirectActionReporter.Definition;
import org.chromium.chrome.browser.directactions.DirectActionReporter.Type;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/** Tests {@link FindInPageDirectActionHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FindInPageDirectActionHandlerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private DirectActionReporter mMockedReporter;

    @Mock
    private AppCompatEditText mMockedEditText;

    @Mock
    private WebContents mWebContents;

    @Mock
    private FindToolbarManager mMockedFindToolbarManager;

    private DirectActionHandler mHandler;

    @Before
    public void setUp() {
        Tab mockTab = mock(Tab.class);
        when(mockTab.isNativePage()).thenReturn(false);
        when(mockTab.getWebContents()).thenReturn(mWebContents);

        TabModelSelector mockTabModelSelector = mock(TabModelSelector.class);
        when(mockTabModelSelector.getCurrentTab()).thenReturn(mockTab);

        when(mMockedReporter.addDirectAction(org.mockito.ArgumentMatchers.anyString()))
                .thenReturn(new FakeDirectActionDefinition("unused"));
        mHandler =
                new FindInPageDirectActionHandler(mockTabModelSelector, mMockedFindToolbarManager);
        mHandler.reportAvailableDirectActions(mMockedReporter);
        verify(mMockedReporter).addDirectAction("find_in_page");
    }

    @Test
    @SmallTest
    public void testWrongActionIdNotHandled() {
        assertFalse(mHandler.performDirectAction("wrong_name", Bundle.EMPTY, null));
        verify(mMockedFindToolbarManager, never()).showToolbar();
    }

    @Test
    @SmallTest
    public void testEmptyStringIgnored() {
        List<Bundle> responses = new ArrayList<>();
        assertTrue(mHandler.performDirectAction(
                "find_in_page", Bundle.EMPTY, (response) -> responses.add(response)));
        assertThat(responses, Matchers.hasSize(1));
        // We still want to bring up the find toolbar.
        verify(mMockedFindToolbarManager).showToolbar();
        // Setting the search string should not happen.
        verify(mMockedFindToolbarManager, never()).setFindQuery(anyString());
    }

    @Test
    @SmallTest
    public void testFindInPageFull() {
        List<Bundle> responses = new ArrayList<>();
        Bundle args = new Bundle();
        args.putString("SEARCH_QUERY", "search string");
        assertTrue(mHandler.performDirectAction(
                "find_in_page", args, (response) -> responses.add(response)));
        assertThat(responses, Matchers.hasSize(1));

        verify(mMockedFindToolbarManager).showToolbar();
        verify(mMockedFindToolbarManager).setFindQuery("search string");
    }

    /**
     * A simple action definition for testing.
     */
    private static class FakeDirectActionDefinition implements Definition {
        final String mId;
        List<FakeParameter> mParameters = new ArrayList<>();
        List<FakeParameter> mResults = new ArrayList<>();

        FakeDirectActionDefinition(String id) {
            mId = id;
        }

        @Override
        public Definition withParameter(String name, @Type int type, boolean required) {
            mParameters.add(new FakeParameter(name, type, required));
            return this;
        }

        @Override
        public Definition withResult(String name, @Type int type) {
            mResults.add(new FakeParameter(name, type, true));
            return this;
        }
    }

    /** A simple parameter definition for testing. */
    private static class FakeParameter {
        final String mName;

        @Type
        final int mType;
        final boolean mRequired;

        FakeParameter(String name, @Type int type, boolean required) {
            mName = name;
            mType = type;
            mRequired = required;
        }
    }
}
