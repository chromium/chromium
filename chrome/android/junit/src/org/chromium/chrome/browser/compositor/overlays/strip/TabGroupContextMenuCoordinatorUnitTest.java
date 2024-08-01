// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

/** Unit tests for {@link TabGroupContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupContextMenuCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    @Ignore("Test not implemented yet")
    public void testListMenuItems() {
        // TODO(crbug.com/354248683): Implement.
    }

    @Test
    @Ignore("Test not implemented yet")
    public void testCustomMenuItems() {
        // TODO(crbug.com/354248683): Implement.
    }
}
