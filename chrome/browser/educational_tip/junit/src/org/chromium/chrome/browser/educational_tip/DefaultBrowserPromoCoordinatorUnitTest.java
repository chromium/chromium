// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link DefaultBrowserPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class DefaultBrowserPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Runnable mOnModuleClickedCallback;

    private DefaultBrowserPromoCoordinator mDefaultBrowserPromoCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mDefaultBrowserPromoCoordinator =
                new DefaultBrowserPromoCoordinator(
                        RuntimeEnvironment.application, mOnModuleClickedCallback);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testClickDefaultBrowserPromoCard() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        mDefaultBrowserPromoCoordinator.onCardClicked();
        verify(mOnModuleClickedCallback, times(1)).run();
    }
}
