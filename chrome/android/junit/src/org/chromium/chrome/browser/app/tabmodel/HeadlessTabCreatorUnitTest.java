// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.content_public.browser.LoadUrlParams;

/** Unit tests for {@link HeadlessTabCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HeadlessTabCreatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;

    private HeadlessTabCreator mDisabledCreator;
    private HeadlessTabCreator mCreator;

    @Before
    public void setUp() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(
                /* isSignedInAndSyncEnabled= */ false);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(/* enabled= */ false);
        TabTestUtils.mockTabJni();

        mDisabledCreator = new HeadlessTabCreator(mProfile, /* isIncognito= */ true);
        mCreator = new HeadlessTabCreator(mProfile, /* isIncognito= */ false);
        mCreator.setTabModel(mTabModel);
    }

    @Test
    public void testDisabledCreatorThrows() {
        LoadUrlParams loadUrlParams = new LoadUrlParams("https://example.com");
        assertThrows(
                UnsupportedOperationException.class,
                () ->
                        mDisabledCreator.createNewTab(
                                loadUrlParams, TabLaunchType.FROM_LINK, /* parent= */ null));
    }

    @Test
    public void testCreateFrozenTabDoesNotCreateWebContents() {
        Tab tab = mCreator.createFrozenTab(/* state= */ null, /* id= */ 1, /* index= */ 0);
        assertNotNull(tab);
        assertNull(tab.getWebContents());
        assertFalse(tab.loadIfNeeded(/* forceBackingSize= */ false));
        assertNull(tab.getWebContents());
        assertFalse(tab.loadIfNeeded(/* forceBackingSize= */ true));
        assertNull(tab.getWebContents());
        verify(mTabModel)
                .addTab(
                        eq(tab),
                        eq(/* index= */ 0),
                        eq(TabLaunchType.FROM_RESTORE),
                        eq(TabCreationState.FROZEN_ON_RESTORE));
    }

    @Test
    public void testCreateNewTabDoesNotCreateWebContents() {
        when(mTabModel.getCount()).thenReturn(0);
        LoadUrlParams loadUrlParams = new LoadUrlParams("https://example.com");
        Tab tab = mCreator.createNewTab(loadUrlParams, TabLaunchType.FROM_LINK, /* parent= */ null);
        assertNotNull(tab);
        assertNull(tab.getWebContents());
        assertFalse(tab.loadIfNeeded(/* forceBackingSize= */ false));
        assertNull(tab.getWebContents());
        assertFalse(tab.loadIfNeeded(/* forceBackingSize= */ true));
        assertNull(tab.getWebContents());
        verify(mTabModel)
                .addTab(
                        eq(tab),
                        eq(/* index= */ 0),
                        eq(TabLaunchType.FROM_LINK),
                        eq(TabCreationState.FROZEN_FOR_LAZY_LOAD));
    }
}
