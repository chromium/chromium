// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.junit.Assert.assertEquals;

import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for {@link SiteSearchSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SiteSearchSettingsUnitTest {
    private SiteSearchSettings mFragment;
    private BlankUiTestActivity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(BlankUiTestActivity.class).setup().get();
        mFragment = new SiteSearchSettings();
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        fragmentManager.beginTransaction().add(mFragment, null).commitNow();
    }

    @Test
    public void testGetPageTitle() {
        assertEquals(
                mActivity.getString(R.string.manage_search_engines_and_site_search),
                mFragment.getPageTitle().get());
    }
}
