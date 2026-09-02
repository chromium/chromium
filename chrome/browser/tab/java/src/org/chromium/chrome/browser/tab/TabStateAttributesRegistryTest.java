// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for TabStateAttributesRegistry. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLooper.class})
public class TabStateAttributesRegistryTest {
    private static class FakeKey1 implements TabStateAttributes.StoreKey {}

    private static class FakeKey2 implements TabStateAttributes.StoreKey {}

    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;

    private MockTab mTab;

    @Before
    public void setUp() {
        mTab =
                new MockTab(0, mProfile) {
                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }

                    @Override
                    public boolean isInitialized() {
                        return true;
                    }
                };
        mTab.setCanGoForward(false);
        mTab.setCanGoBack(false);
    }

    @Test
    public void testMultipleInstances() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, FakeKey1.class, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, FakeKey2.class, TabCreationState.FROZEN_ON_RESTORE);

        TabStateAttributes attrs1 =
                TabStateAttributesRegistry.getAttributesFor(mTab, FakeKey1.class);
        TabStateAttributes attrs2 =
                TabStateAttributesRegistry.getAttributesFor(mTab, FakeKey2.class);

        attrs1.updateIsDirty(DirtinessState.DIRTY);
        assertEquals(DirtinessState.DIRTY, attrs1.getDirtinessState());
        assertEquals(DirtinessState.CLEAN, attrs2.getDirtinessState());

        attrs2.updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.DIRTY, attrs1.getDirtinessState());
        assertEquals(DirtinessState.UNTIDY, attrs2.getDirtinessState());
    }

    @Test
    public void testGetAllAttributes() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, FakeKey1.class, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, FakeKey2.class, TabCreationState.FROZEN_ON_RESTORE);

        TabStateAttributesRegistry registry =
                mTab.getUserDataHost().getUserData(TabStateAttributesRegistry.class);
        assertEquals(2, registry.getAllAttributes().size());
    }
}
