// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.invocation.InvocationOnMock;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ResourceTextResolver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ResourceTextResolverUnitTest {

    @Test
    @SmallTest
    public void testResolveString() {
        Context mockContext = mock(Context.class);
        Resources mockResources = mock(Resources.class);
        doReturn(mockResources).when(mockContext).getResources();

        int stringResId = 123;
        String expectedString = "Expected String";
        doReturn(expectedString).when(mockContext).getString(eq(stringResId));

        ResourceTextResolver resolver = new ResourceTextResolver(stringResId);
        assertEquals(expectedString, resolver.resolve(mockContext));
    }

    @Test
    @SmallTest
    public void testResolvePluralString() {
        Context mockContext = mock(Context.class);
        Resources mockResources = mock(Resources.class);
        doReturn(mockResources).when(mockContext).getResources();

        int pluralResId = 456;
        when(mockResources.getQuantityString(eq(pluralResId), anyInt(), anyInt()))
                .thenAnswer(
                        (InvocationOnMock invocation) -> {
                            int count = invocation.getArgument(1);
                            return count == 1 ? "1 item" : count + " items";
                        });

        int[] counts = {0, 1, 2, 5, 10, 100};

        for (int count : counts) {
            String expectedString = count == 1 ? "1 item" : count + " items";
            ResourceTextResolver resolver = new ResourceTextResolver(pluralResId, count);
            assertEquals(expectedString, resolver.resolve(mockContext));
        }
    }
}
