// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.ui.modelutil.PropertyModel;

/** Logic for the toolbar of the Hub. */
public class HubToolbarMediator {
    private final PropertyModel mPropertyModel;

    /** Creates the mediator. */
    public HubToolbarMediator(PropertyModel propertyModel) {
        mPropertyModel = propertyModel;
    }

    /** Cleans up observers. */
    public void destroy() {}
}
