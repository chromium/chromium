// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.tips.GoogleLensTipsPromoHandler;
import org.chromium.chrome.browser.quick_delete.tips.QuickDeleteTipsPromoHandler;
import org.chromium.chrome.browser.safe_browsing.tips.EnhancedSafeBrowsingTipsPromoHandler;
import org.chromium.chrome.browser.toolbar.tips.BottomOmniboxTipsPromoHandler;

/** Unit tests for {@link TipsPromoHandlerRegistrar}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsPromoHandlerRegistrarUnitTest {
    @After
    public void tearDown() {
        TipsPromoHandlerFactory.resetForTesting();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testRegisterAll_RegistersEnhancedSafeBrowsingHandler() {
        assertNull(
                TipsPromoHandlerFactory.getHandler(
                        TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING));

        TipsPromoHandlerRegistrar.registerAll();

        TipsPromoHandler handler =
                TipsPromoHandlerFactory.getHandler(
                        TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING);
        assertNotNull(handler);
        assertTrue(handler instanceof EnhancedSafeBrowsingTipsPromoHandler);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testRegisterAll_RegistersQuickDeleteHandler() {
        assertNull(TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.QUICK_DELETE));

        TipsPromoHandlerRegistrar.registerAll();

        TipsPromoHandler handler =
                TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.QUICK_DELETE);
        assertNotNull(handler);
        assertTrue(handler instanceof QuickDeleteTipsPromoHandler);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testRegisterAll_RegistersGoogleLensHandler() {
        assertNull(TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.GOOGLE_LENS));

        TipsPromoHandlerRegistrar.registerAll();

        TipsPromoHandler handler =
                TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.GOOGLE_LENS);
        assertNotNull(handler);
        assertTrue(handler instanceof GoogleLensTipsPromoHandler);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testRegisterAll_RegistersBottomOmniboxHandler() {
        assertNull(TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.BOTTOM_OMNIBOX));

        TipsPromoHandlerRegistrar.registerAll();

        TipsPromoHandler handler =
                TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.BOTTOM_OMNIBOX);
        assertNotNull(handler);
        assertTrue(handler instanceof BottomOmniboxTipsPromoHandler);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testRegisterAll_IsIdempotent() {
        TipsPromoHandlerRegistrar.registerAll();
        TipsPromoHandlerRegistrar.registerAll();

        assertNotNull(
                TipsPromoHandlerFactory.getHandler(
                        TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testRegisterAll_HandlerNotReturnedWhenSelfServiceDisabled() {
        TipsPromoHandlerRegistrar.registerAll();

        assertNull(
                TipsPromoHandlerFactory.getHandler(
                        TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING));
    }
}
