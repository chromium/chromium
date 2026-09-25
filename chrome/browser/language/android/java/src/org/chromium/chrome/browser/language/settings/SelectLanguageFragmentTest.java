// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.fragment.app.FragmentFactory;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link SelectLanguageFragment} and its per-selection subclasses. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectLanguageFragmentTest {
    @Test
    public void testEveryPickerOffersADistinctList() {
        Set<Integer> listTypes = new HashSet<>();
        for (Class<? extends SelectLanguageFragment> picker : SelectLanguageFragment.ALL_PICKERS) {
            assertTrue(
                    "Duplicate language list for " + picker.getName(),
                    listTypes.add(instantiate(picker).getLanguageListType()));
        }
        assertEquals(
                Set.of(
                        LanguagesManager.LanguageListType.UI_LANGUAGES,
                        LanguagesManager.LanguageListType.ACCEPT_LANGUAGES,
                        LanguagesManager.LanguageListType.TARGET_LANGUAGES,
                        LanguagesManager.LanguageListType.ALWAYS_LANGUAGES,
                        LanguagesManager.LanguageListType.NEVER_LANGUAGES),
                listTypes);
    }

    @Test
    public void testTranslateListsOpenTheirOwnPicker() {
        assertEquals(
                SelectLanguageFragment.AlwaysTranslateLanguagePickerFragment.class,
                new AlwaysTranslateListFragment().getLanguagePickerClass());
        assertEquals(
                SelectLanguageFragment.NeverTranslateLanguagePickerFragment.class,
                new NeverTranslateListFragment().getLanguagePickerClass());
    }

    /**
     * Instantiates {@code picker} by name, as the fragment framework does when showing it or
     * restoring it, which requires a public class with a public no-argument constructor.
     */
    private static SelectLanguageFragment instantiate(
            Class<? extends SelectLanguageFragment> picker) {
        return (SelectLanguageFragment)
                new FragmentFactory()
                        .instantiate(
                                RuntimeEnvironment.getApplication().getClassLoader(),
                                picker.getName());
    }
}
