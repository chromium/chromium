// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ReadAloudController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadAloudControllerUnitTest {
    private static final GURL sTestGURL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

    private MockTab mTab;
    private ReadAloudController mController;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Mock
    private ObservableSupplier<Profile> mMockProfileSupplier;
    @Mock
    private Profile mMockProfile;

    MockTabModelSelector mTabModelSelector;

    @Captor
    ArgumentCaptor<ReadAloudReadabilityHooks.ReadabilityCallback> mCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mMockProfile).when(mMockProfileSupplier).get();

        when(mMockProfile.isOffTheRecord()).thenReturn(false);
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(true);

        mTabModelSelector = new MockTabModelSelector(
                /* tabCount= */ 2, /* incognitoTabCount= */ 1, (id, incognito) -> {
                    Tab tab = spy(MockTab.createAndInitialize(id, incognito));
                    return tab;
                });
        mController =
                new ReadAloudController(mMockProfileSupplier, mTabModelSelector.getModel(false));

        mTab = (MockTab) mTabModelSelector.getCurrentTab();
        mTab.setGurlOverrideForTesting(sTestGURL);
    }

    @Test
    public void testIsAvailable() {
        // test set up: non incognito profile + MSBB Accepted
        assertTrue(mController.isAvailable());
    }

    @Test
    public void testIsAvailable_offTheRecord() {
        when(mMockProfile.isOffTheRecord()).thenReturn(true);
        assertFalse(mController.isAvailable());
    }

    @Test
    public void testIsAvailable_noMSBB() {
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(false);
        assertFalse(mController.isAvailable());
    }
}
