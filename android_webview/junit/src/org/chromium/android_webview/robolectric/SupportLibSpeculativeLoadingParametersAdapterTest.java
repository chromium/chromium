// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import androidx.test.filters.SmallTest;

import com.android.webview.chromium.PrefetchParams;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.support_lib_boundary.SpeculativeLoadingParametersBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibSpeculativeLoadingParametersAdapter;

import java.util.Collections;

/** Tests for SupportLibSpeculativeLoadingParametersAdapter. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SupportLibSpeculativeLoadingParametersAdapterTest {

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFromBoundaryInterface_WithVariationsId() {
        SpeculativeLoadingParametersBoundaryInterface mockBoundary =
                Mockito.mock(SpeculativeLoadingParametersBoundaryInterface.class);
        Mockito.when(mockBoundary.getVariationsId()).thenReturn(123);
        Mockito.when(mockBoundary.getAdditionalHeaders()).thenReturn(Collections.emptyMap());
        Mockito.when(mockBoundary.getNoVarySearchData()).thenReturn(null);
        Mockito.when(mockBoundary.isJavaScriptEnabled()).thenReturn(true);

        PrefetchParams params =
                SupportLibSpeculativeLoadingParametersAdapter
                        .fromSpeculativeLoadingParametersBoundaryInterface(mockBoundary);

        Assert.assertEquals(Integer.valueOf(123), params.variationsId);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFromBoundaryInterface_WithoutVariationsId() {
        SpeculativeLoadingParametersBoundaryInterface mockBoundary =
                Mockito.mock(SpeculativeLoadingParametersBoundaryInterface.class);
        // Simulate version skew where the method fails or is missing.
        Mockito.when(mockBoundary.getVariationsId())
                .thenThrow(new RuntimeException("Simulated missing method"));
        Mockito.when(mockBoundary.getAdditionalHeaders()).thenReturn(Collections.emptyMap());
        Mockito.when(mockBoundary.getNoVarySearchData()).thenReturn(null);
        Mockito.when(mockBoundary.isJavaScriptEnabled()).thenReturn(true);

        // This should NOT crash and should return null for variationsId.
        PrefetchParams params =
                SupportLibSpeculativeLoadingParametersAdapter
                        .fromSpeculativeLoadingParametersBoundaryInterface(mockBoundary);

        Assert.assertNull(params.variationsId);
    }
}
