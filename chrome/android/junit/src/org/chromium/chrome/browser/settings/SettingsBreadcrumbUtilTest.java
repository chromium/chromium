// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.os.Bundle;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link SettingsBreadcrumbUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsBreadcrumbUtilTest {
    @After
    public void tearDown() {
        SettingsIndexData.reset();
    }

    @Test
    public void testGetInitialBreadcrumbPath_nullBundle() {
        assertNull(SettingsBreadcrumbUtil.getInitialBreadcrumbPath(null));
    }

    @Test
    public void testGetInitialBreadcrumbPath_emptyBundle() {
        assertNull(SettingsBreadcrumbUtil.getInitialBreadcrumbPath(new Bundle()));
    }

    @Test
    public void testSaveAndGetInitialBreadcrumbPath_roundTrip() {
        Bundle bundle = new Bundle();
        List<SettingsIndexData.Entry> entries = new ArrayList<>();
        entries.add(
                new SettingsIndexData.Entry.Builder("id_1", "key_1", "Title 1", "FragmentClass1")
                        .build());
        entries.add(
                new SettingsIndexData.Entry.Builder("id_2", "key_2", "Title 2", "FragmentClass2")
                        .build());

        SettingsBreadcrumbUtil.saveInitialBreadcrumbPath(bundle, entries);
        assertTrue(bundle.containsKey(SettingsBreadcrumbUtil.KEY_INITIAL_BREADCRUMB_PATH));

        List<SettingsIndexData.Entry> restored =
                SettingsBreadcrumbUtil.getInitialBreadcrumbPath(bundle);
        assertNotNull(restored);
        assertEquals(2, restored.size());
        assertEquals("id_1", restored.get(0).id);
        assertEquals("id_2", restored.get(1).id);
    }

    @Test
    public void testSaveInitialBreadcrumbPath_nullPath() {
        Bundle bundle = new Bundle();
        SettingsBreadcrumbUtil.saveInitialBreadcrumbPath(bundle, null);
        assertFalse(bundle.containsKey(SettingsBreadcrumbUtil.KEY_INITIAL_BREADCRUMB_PATH));
    }

    @Test
    public void testGetInitialBreadcrumbPath_nullFragmentName() {
        Context context = ApplicationProvider.getApplicationContext();
        Profile profile = mock(Profile.class);
        assertNull(SettingsBreadcrumbUtil.getInitialBreadcrumbPath(context, profile, null, null));
    }

    @Test
    public void testGetInitialBreadcrumbPath_fromFragment_noPath() {
        Context context = ApplicationProvider.getApplicationContext();
        Profile profile = mock(Profile.class);
        SettingsIndexData indexData = SettingsIndexData.createInstance();
        indexData.resetNeedsIndexing();

        assertNull(
                SettingsBreadcrumbUtil.getInitialBreadcrumbPath(
                        context, profile, "NonExistentFragment", null));
    }
}
