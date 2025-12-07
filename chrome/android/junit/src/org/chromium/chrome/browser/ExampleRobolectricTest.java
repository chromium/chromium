// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;

/**
 * Example [Robolectric] [unit] test.
 *
 * <p>All Roboloectric tests are batched.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExampleRobolectricTest {
    static class ExampleClassUnderTest {
        private final TabContentManager mTabContentManager;

        ExampleClassUnderTest(TabContentManager tabContentManager) {
            mTabContentManager = tabContentManager;
        }

        public void callCacheTabThumbnail(Tab tab) {
            mTabContentManager.cacheTabThumbnail(tab);
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Profile mProfile;
    @Mock TabContentManager mTabContentManager;

    @Test
    public void callCacheTabThumbnail() {
        Tab tab = new MockTab(42, mProfile);
        ExampleClassUnderTest subject = new ExampleClassUnderTest(mTabContentManager);

        subject.callCacheTabThumbnail(tab);

        verify(mTabContentManager).cacheTabThumbnail(tab);
    }
}
