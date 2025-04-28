// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.app.Activity;

import org.chromium.base.BuildInfo;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;

public class FullscreenHtmlApiHandlerFactory {

    /** Creates an instance of {@link FullscreenHtmlApiHandlerBase}. */
    static FullscreenHtmlApiHandlerBase createInstance(
            Activity activity,
            ObservableSupplier<Boolean> areControlsHidden,
            boolean exitFullscreenOnStop,
            MultiWindowModeStateDispatcher multiWindowDispatcher) {
        if (isFullscreenApiMigrationEnabled()) {
            return new FullscreenHtmlApiHandlerCompat(
                    activity, areControlsHidden, exitFullscreenOnStop, multiWindowDispatcher);
        }
        return new FullscreenHtmlApiHandlerLegacy(
                activity, areControlsHidden, exitFullscreenOnStop, multiWindowDispatcher);
    }

    private static boolean isFullscreenApiMigrationEnabled() {
        return ChromeFeatureList.sFullscreenInsetsApiMigration.isEnabled()
                || (BuildInfo.getInstance().isAutomotive
                        && ChromeFeatureList.sFullscreenInsetsApiMigrationOnAutomotive.isEnabled());
    }
}
