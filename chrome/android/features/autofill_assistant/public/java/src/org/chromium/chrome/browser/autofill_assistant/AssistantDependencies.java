// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

/**
 * Generic dependencies interface. The concrete implementation will depend on the browser framework,
 * i.e., WebLayer vs. Chrome.
 */
public interface AssistantDependencies extends AssistantStaticDependencies {
    WebContents getWebContents();

    Context getContext();

    BottomSheetController getBottomSheetController();

    BrowserControlsStateProvider getBrowserControls();

    KeyboardVisibilityDelegate getKeyboardVisibilityDelegate();

    ApplicationViewportInsetSupplier getBottomInsetProvider();

    ActivityTabProvider getActivityTabProvider();

    View getRootView();

    AssistantSnackbarFactory getSnackbarFactory();
}
