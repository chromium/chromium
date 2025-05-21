// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.provider.Settings;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric Unit Tests for {@link PackageUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PackageUtilsUnitTest {

    @Test
    public void testGetDefaultAssistantPackageName() {
        var context = mock(Context.class);
        var contentResolver = mock(ContentResolver.class);
        when(context.getContentResolver()).thenReturn(contentResolver);
        Settings.Secure.putString(
                contentResolver, "assistant", "com.example.app/com.example.app.Assistant");

        var result = PackageUtils.getDefaultAssistantPackageName(context);
        assertEquals("com.example.app", result);
    }

    @Test
    public void testGetDefaultAssistantPackageName_wrongFormat() {
        var context = mock(Context.class);
        var contentResolver = mock(ContentResolver.class);
        when(context.getContentResolver()).thenReturn(contentResolver);
        Settings.Secure.putString(contentResolver, "assistant", "String with invalid format");

        var result = PackageUtils.getDefaultAssistantPackageName(context);
        assertNull(result);
    }
}
