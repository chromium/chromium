// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link TabCollectionTabModelImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCollectionTabModelImplUnitTest {
    private static final long NATIVE_PTR = 875943L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelJniBridge.Natives mTabModelJniBridgeJni;
    @Mock private Profile mProfile;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private TabCreator mIncognitoTabCreator;

    private TabCollectionTabModelImpl mTabModel;

    @Before
    public void setUp() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfile.isIncognitoBranded()).thenReturn(false);
        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridgeJni);
        when(mTabModelJniBridgeJni.init(
                        any(),
                        eq(mProfile),
                        eq(ActivityType.TABBED),
                        /* isArchivedTabModel= */ eq(false)))
                .thenReturn(NATIVE_PTR);

        mTabModel =
                new TabCollectionTabModelImpl(
                        mProfile,
                        ActivityType.TABBED,
                        /* isArchivedTabModel= */ false,
                        mRegularTabCreator,
                        mIncognitoTabCreator);
    }

    @After
    public void tearDown() {
        mTabModel.destroy();
        verify(mTabModelJniBridgeJni).destroy(eq(NATIVE_PTR), any());
    }

    @Test
    public void testGetTabCreator() {
        assertEquals(mRegularTabCreator, mTabModel.getTabCreator());
    }

    @Test
    public void testBroadcastSessionRestoreComplete() {
        mTabModel.broadcastSessionRestoreComplete();

        verify(mTabModelJniBridgeJni).broadcastSessionRestoreComplete(eq(NATIVE_PTR), any());
    }
}
