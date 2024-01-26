// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.app.Activity;

import org.chromium.base.supplier.ObservableSupplier;

public class FullscreenHtmlApiHandlerFactory {

    /** Creates an instance of {@link FullscreenHtmlApiHandlerBase}. */
    static FullscreenHtmlApiHandlerBase createInstance(
            Activity activity,
            ObservableSupplier<Boolean> areControlsHidden,
            boolean exitFullscreenOnStop) {
        return new FullscreenHtmlApiHandlerLegacy(
                activity, areControlsHidden, exitFullscreenOnStop);
    }
}
