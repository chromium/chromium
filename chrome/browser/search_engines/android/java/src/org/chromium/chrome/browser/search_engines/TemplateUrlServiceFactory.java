// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.search_engines.TemplateUrlService;

/**
 * This factory links the native TemplateURLService for the current Profile to create and hold a
 * {@link TemplateUrlService} singleton.
 */
public class TemplateUrlServiceFactory {
    private static TemplateUrlService sTemplateUrlService;

    private TemplateUrlServiceFactory() {}

    /**
     * @return The singleton instance of {@link TemplateUrlService}, creating it if necessary.
     */
    public static TemplateUrlService get() {
        ThreadUtils.assertOnUiThread();
        if (sTemplateUrlService == null) {
            sTemplateUrlService = TemplateUrlServiceFactoryJni.get().getTemplateUrlService();
        }
        return sTemplateUrlService;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(TemplateUrlService service) {
        sTemplateUrlService = service;
    }

    /**
     * TODO(crbug.com/968156): Move to TemplateUrlServiceHelper.
     */
    public static boolean doesDefaultSearchEngineHaveLogo() {
        return TemplateUrlServiceFactoryJni.get().doesDefaultSearchEngineHaveLogo();
    }

    // Natives interface is public to allow mocking in tests outside of
    // org.chromium.chrome.browser.search_engines package.
    @NativeMethods
    public interface Natives {
        TemplateUrlService getTemplateUrlService();
        boolean doesDefaultSearchEngineHaveLogo();
    }
}
