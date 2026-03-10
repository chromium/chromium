// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link RecordingTabCreatorManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
public class RecordingTabCreatorManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabCreatorManager mDelegate;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private TabCreator mIncognitoTabCreator;

    private RecordingTabCreatorManager mManager;

    @Before
    public void setUp() {
        when(mDelegate.getTabCreator(false)).thenReturn(mRegularTabCreator);
        when(mDelegate.getTabCreator(true)).thenReturn(mIncognitoTabCreator);
        mManager = new RecordingTabCreatorManager(mDelegate);
    }

    @Test
    public void testGetTabCreator_Regular() {
        TabCreator creator = mManager.getTabCreator(false);
        assertTrue(creator instanceof RecordingTabCreator);
        assertNotNull(mManager.getRecorder(false));
    }

    @Test
    public void testGetTabCreator_Incognito() {
        TabCreator creator = mManager.getTabCreator(true);
        assertEquals(mIncognitoTabCreator, creator);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testGetTabCreator_Regular_FeatureDisabled() {
        TabCreator creator = mManager.getTabCreator(false);
        assertEquals(mRegularTabCreator, creator);
        assertNull(mManager.getRecorder(false));
    }
}
