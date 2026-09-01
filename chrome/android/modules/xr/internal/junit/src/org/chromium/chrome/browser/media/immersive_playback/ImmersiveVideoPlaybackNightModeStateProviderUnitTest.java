// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoPlaybackActivity.ImmersiveVideoPlaybackNightModeStateProvider;

/** Unit tests for {@link ImmersiveVideoPlaybackNightModeStateProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImmersiveVideoPlaybackNightModeStateProviderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AppCompatDelegate mDelegate;
    private ImmersiveVideoPlaybackNightModeStateProvider mProvider;

    @Before
    public void setup() {
        mProvider = new ImmersiveVideoPlaybackNightModeStateProvider();
    }

    @Test
    public void testInitialize() {
        mProvider.initialize(mDelegate);
        verify(mDelegate).setLocalNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        Assert.assertTrue(mProvider.isInNightMode());
    }
}
