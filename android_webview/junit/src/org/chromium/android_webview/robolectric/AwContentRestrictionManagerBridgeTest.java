// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwContentRestrictionManagerBridge;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;

/** Unit tests for the AwContentRestrictionManagerBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwContentRestrictionManagerBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock AconfigFlaggedApiDelegate mFlaggedApiDelegate;

    @Before
    public void setUp() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testIsContentRestrictionEnabled_featureDisabled() {
        Assert.assertFalse(AwContentRestrictionManagerBridge.isContentRestrictionEnabled());
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testIsContentRestrictionEnabled_featureEnabled() {
        when(mFlaggedApiDelegate.isContentRestrictionEnabled()).thenReturn(true);
        Assert.assertTrue(AwContentRestrictionManagerBridge.isContentRestrictionEnabled());
        verify(mFlaggedApiDelegate).isContentRestrictionEnabled();

        when(mFlaggedApiDelegate.isContentRestrictionEnabled()).thenReturn(false);
        Assert.assertFalse(AwContentRestrictionManagerBridge.isContentRestrictionEnabled());
        verify(mFlaggedApiDelegate, times(2)).isContentRestrictionEnabled();
    }
}
