// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ResourceTextResolver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ResourceTextResolverUnitTest {
    private static final int STRING_RES_ID = 123;
    private static final int PLURAL_RES_ID = 456;
    private static final String STRING_VALUE = "Test String";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private Resources mResources;

    @Before
    public void setUp() {
        when(mContext.getResources()).thenReturn(mResources);
        when(mContext.getString(STRING_RES_ID)).thenReturn(STRING_VALUE);
        when(mResources.getQuantityString(eq(PLURAL_RES_ID), anyInt(), anyInt()))
                .thenAnswer(
                        inv -> {
                            int count = inv.getArgument(1);
                            return count == 1 ? "1 item" : count + " items";
                        });
    }

    @Test
    @SmallTest
    public void testResolveString() {
        ResourceTextResolver resolver = new ResourceTextResolver(STRING_RES_ID);
        assertEquals(STRING_VALUE, resolver.resolve(mContext));
    }

    @Test
    @SmallTest
    public void testResolvePluralString() {
        int[] counts = {0, 1, 2, 5, 10, 100};

        ResourceTextResolver resolver = null;
        for (int count : counts) {
            String expected = count == 1 ? "1 item" : count + " items";
            resolver = new ResourceTextResolver(PLURAL_RES_ID, count);
            assertEquals(expected, resolver.resolve(mContext));
        }

        int lastCount = counts[counts.length - 1];
        String expected = lastCount + " items";
        assertEquals(expected, resolver.resolve(mContext));
        verify(mResources, times(1)).getQuantityString(PLURAL_RES_ID, lastCount, lastCount);
    }

    @Test
    @SmallTest
    public void testResolveNull_returnsEmptyString() {
        ResourceTextResolver resolver = new ResourceTextResolver(Resources.ID_NULL);
        assertEquals("", resolver.resolve(mContext));
    }
}
