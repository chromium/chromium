// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link ArchivedTabModelSelectorHolder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ArchivedTabModelSelectorHolderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;

    @Before
    public void setUp() {
        ArchivedTabModelSelectorHolder.setInstanceFn(/* archivedTabModelSelectorFn= */ null);
    }

    @After
    public void tearDown() {
        ArchivedTabModelSelectorHolder.setInstanceFn(/* archivedTabModelSelectorFn= */ null);
    }

    @Test
    public void testGetInstance_NullProfile() {
        ArchivedTabModelSelectorHolder.setInstanceFn((profile) -> mTabModelSelector);
        assertNull(ArchivedTabModelSelectorHolder.getInstance(/* profile= */ null));
    }

    @Test
    public void testGetInstance_NullFunction() {
        ArchivedTabModelSelectorHolder.setInstanceFn(/* archivedTabModelSelectorFn= */ null);
        assertNull(ArchivedTabModelSelectorHolder.getInstance(mProfile));
    }

    @Test
    public void testGetInstance_FunctionReturnsNull() {
        ArchivedTabModelSelectorHolder.setInstanceFn((profile) -> null);
        assertNull(ArchivedTabModelSelectorHolder.getInstance(mProfile));
    }

    @Test
    public void testGetInstance_Success() {
        ArchivedTabModelSelectorHolder.setInstanceFn((profile) -> mTabModelSelector);
        assertEquals(mTabModelSelector, ArchivedTabModelSelectorHolder.getInstance(mProfile));
    }
}
