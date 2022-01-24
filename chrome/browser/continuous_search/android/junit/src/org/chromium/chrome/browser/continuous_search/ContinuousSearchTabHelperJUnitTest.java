// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/**
 * Test of {@link ContinuousSearchTabHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchTabHelperJUnitTest {
    @Mock
    private Tab mTabMock;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Before
    public void setUp() {
        Context context = mock(Context.class);
        Resources resources = mock(Resources.class);
        when(mTabMock.getContext()).thenReturn(context);
        when(context.getResources()).thenReturn(resources);
        when(resources.getInteger(anyInt())).thenReturn(0);
    }

    /**
     * Tests successful initialization of the tab observer.
     */
    @Test
    @EnableFeatures({ChromeFeatureList.CONTINUOUS_SEARCH})
    public void testInitializeSuccess() {
        when(mTabMock.isIncognito()).thenReturn(false);

        ContinuousSearchTabHelper.createForTab(mTabMock);

        verify(mTabMock).addObserver(any());
    }

    /**
     * Tests initialization is skipped for incognito tabs even if the feature is on.
     */
    @Test
    @EnableFeatures({ChromeFeatureList.CONTINUOUS_SEARCH})
    public void testNoInitializeIfIncognito() {
        when(mTabMock.isIncognito()).thenReturn(true);

        ContinuousSearchTabHelper.createForTab(mTabMock);

        verify(mTabMock, never()).addObserver(any());
    }

    /**
     * Tests skip initialization if the feature flag is off.
     */
    @Test
    @DisableFeatures({ChromeFeatureList.CONTINUOUS_SEARCH})
    public void testFeatureDisabled() {
        when(mTabMock.isIncognito()).thenReturn(false);

        ContinuousSearchTabHelper.createForTab(mTabMock);

        verify(mTabMock, never()).addObserver(any());
    }
}
