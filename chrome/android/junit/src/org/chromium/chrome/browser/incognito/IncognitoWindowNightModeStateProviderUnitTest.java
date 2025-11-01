// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.mockito.Mockito.verify;

import androidx.appcompat.app.AppCompatDelegate;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link IncognitoWindowNightModeStateProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IncognitoWindowNightModeStateProviderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AppCompatDelegate mDelegate;
    private IncognitoWindowNightModeStateProvider mProvider;

    @Before
    public void setup() {
        mProvider = new IncognitoWindowNightModeStateProvider();
    }

    @Test
    public void testInitialize() {
        mProvider.initialize(mDelegate);
        verify(mDelegate).setLocalNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        Assert.assertTrue(mProvider.isInNightMode());
    }
}
