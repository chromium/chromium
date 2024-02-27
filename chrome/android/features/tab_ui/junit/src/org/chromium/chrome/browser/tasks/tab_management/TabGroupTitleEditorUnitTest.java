// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;


import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;

import java.util.HashMap;
import java.util.Map;

/** Tests for {@link TabGroupTitleEditor}. */
@SuppressWarnings({"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored"})
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupTitleEditorUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private Map<String, String> mStorage;
    private TabGroupTitleEditor mTabGroupTitleEditor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTabGroupTitleEditor =
                new TabGroupTitleEditor(RuntimeEnvironment.application) {
                    @Override
                    protected void updateTabGroupTitle(Tab tab, String title) {}

                    @Override
                    protected void storeTabGroupTitle(int tabRootId, String title) {
                        mStorage.put(String.valueOf(tabRootId), title);
                    }

                    @Override
                    protected void deleteTabGroupTitle(int tabRootId) {
                        mStorage.remove(String.valueOf(tabRootId));
                    }

                    @Override
                    protected String getTabGroupTitle(int tabRootId) {
                        return mStorage.get(String.valueOf(tabRootId));
                    }
                };
        mStorage = new HashMap<>();
    }

    @Test
    public void testDefaultTitle() {
        int relatedTabCount = 5;

        String expectedTitle =
                RuntimeEnvironment.application
                        .getResources()
                        .getQuantityString(
                                R.plurals.bottom_tab_grid_title_placeholder,
                                relatedTabCount,
                                relatedTabCount);
        assertEquals(
                expectedTitle,
                TabGroupTitleEditor.getDefaultTitle(
                        RuntimeEnvironment.application, relatedTabCount));
    }

    @Test
    public void testIsDefaultTitle() {
        int fourTabsCount = 4;
        String fourTabsTitle =
                TabGroupTitleEditor.getDefaultTitle(RuntimeEnvironment.application, fourTabsCount);
        assertTrue(mTabGroupTitleEditor.isDefaultTitle(fourTabsTitle, fourTabsCount));
        assertFalse(mTabGroupTitleEditor.isDefaultTitle(fourTabsTitle, 3));
        assertFalse(mTabGroupTitleEditor.isDefaultTitle("Foo", fourTabsCount));
    }
}
