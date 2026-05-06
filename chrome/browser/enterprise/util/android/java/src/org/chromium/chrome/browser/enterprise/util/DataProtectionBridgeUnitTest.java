// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link DataProtectionBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DataProtectionBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DataProtectionBridge.Natives mDataProtectionBridgeJniMock;
    @Mock private WebContents mWebContents;
    @Mock private Runnable mCallback;

    @Before
    public void setUp() {
        DataProtectionBridge.setInstanceForTesting(mDataProtectionBridgeJniMock);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.DATA_CONTROLS_SEARCH_WITH)
    public void testIsSearchWithAllowed_FeatureEnabled() {
        when(mDataProtectionBridgeJniMock.isSearchWithAllowed(mWebContents)).thenReturn(true);
        assertTrue(DataProtectionBridge.isSearchWithAllowed(mWebContents));
        verify(mDataProtectionBridgeJniMock).isSearchWithAllowed(mWebContents);

        when(mDataProtectionBridgeJniMock.isSearchWithAllowed(mWebContents)).thenReturn(false);
        assertFalse(DataProtectionBridge.isSearchWithAllowed(mWebContents));
        verify(mDataProtectionBridgeJniMock, times(2)).isSearchWithAllowed(mWebContents);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.DATA_CONTROLS_SEARCH_WITH)
    public void testIsSearchWithAllowed_FeatureDisabled() {
        assertTrue(DataProtectionBridge.isSearchWithAllowed(mWebContents));
        verify(mDataProtectionBridgeJniMock, never()).isSearchWithAllowed(mWebContents);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.DATA_CONTROLS_SEARCH_WITH)
    public void testShouldAllowSearchWith_FeatureEnabled() {
        DataProtectionBridge.shouldAllowSearchWith(10, mWebContents, mCallback);
        verify(mDataProtectionBridgeJniMock).shouldAllowSearchWith(10, mWebContents, mCallback);
        verify(mCallback, never()).run();
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.DATA_CONTROLS_SEARCH_WITH)
    public void testShouldAllowSearchWith_FeatureDisabled() {
        DataProtectionBridge.shouldAllowSearchWith(10, mWebContents, mCallback);
        verify(mDataProtectionBridgeJniMock, never())
                .shouldAllowSearchWith(10, mWebContents, mCallback);
        verify(mCallback).run();
    }
}
