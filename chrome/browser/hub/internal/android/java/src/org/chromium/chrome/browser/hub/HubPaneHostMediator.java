// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.ui.modelutil.PropertyModel;

/** Logic for hosting a single pane at a time in the Hub. */
public class HubPaneHostMediator {
    private final PropertyModel mPropertyModel;

    /** Creates the mediator. */
    public HubPaneHostMediator(PropertyModel propertyModel) {
        mPropertyModel = propertyModel;
    }

    /** Cleans up observers. */
    public void destroy() {}
}
