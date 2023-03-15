// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

/**
 * Controller for accessing an instance of the RestoreTabsFeatureHelper for the singleton factory
 * instance.
 */
public class RestoreTabsControllerImpl {
    private RestoreTabsFeatureHelper mHelper;

    public RestoreTabsControllerImpl() {
        mHelper = new RestoreTabsFeatureHelperImpl();
    }

    public RestoreTabsFeatureHelper getFeatureHelper() {
        return mHelper;
    }
}