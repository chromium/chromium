// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.safe_browsing.tips.EnhancedSafeBrowsingTipsPromoHandler;

/**
 * Registers the self-service {@link TipsPromoHandler} implementations owned by individual feature
 * modules with the {@link TipsPromoHandlerFactory}. This is the Java counterpart of the native
 * feature registration in {@code TipsServiceFactory}.
 *
 * <p>Lives in the top-level browser target because it must depend on every feature module that
 * provides a handler, while those modules depend on {@code //chrome/browser/tips/android}.
 */
@NullMarked
public final class TipsPromoHandlerRegistrar {
    private TipsPromoHandlerRegistrar() {}

    /** Registers all self-service handlers. Safe to call more than once. */
    public static void registerAll() {
        TipsPromoHandlerFactory.register(
                TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING,
                EnhancedSafeBrowsingTipsPromoHandler::new);
    }
}
