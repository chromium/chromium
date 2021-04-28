// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

/**
 * Factory for creating instances of the AttributionIntentHandler from the attribution_reporting
 * module.
 */
public class AttributionIntentHandlerFactory {
    /**
     * @return a AttributionIntentHandler instance.
     */
    public static AttributionIntentHandler create() {
        return new NoopAttributionIntentHandler();
    }
}
