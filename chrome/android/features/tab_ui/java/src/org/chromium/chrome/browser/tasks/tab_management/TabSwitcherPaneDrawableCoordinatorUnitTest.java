// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertNotNull;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Unit tests for {@link TabSwitcherPaneDrawableCoordinator}. Note these tests are largely for
 * coverage and the bulk of the logic is covered in the mediator test.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneDrawableCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;

    private Context mContext;
    private TabSwitcherPaneDrawableCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        // Needed for TabSwitcherDrawable to inflate correctly with SemanticColorUtils.
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator = new TabSwitcherPaneDrawableCoordinator(mContext, mTabModelSelector);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    @SmallTest
    public void testGetTabSwitcherDrawable() {
        assertNotNull(mCoordinator.getTabSwitcherDrawable());
    }
}
