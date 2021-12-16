// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

/**
 * Generic dependencies interface. The concrete implementation will depend on the browser framework,
 * i.e., WebLayer vs. Chrome.
 *
 * WebContents should not be returned in this interface as objects should stay valid when
 * WebContents change.
 */
@JNINamespace("autofill_assistant")
public interface AssistantDependencies extends AssistantStaticDependencies {
    Activity getActivity();

    BottomSheetController getBottomSheetController();

    BrowserControlsStateProvider getBrowserControls();

    KeyboardVisibilityDelegate getKeyboardVisibilityDelegate();

    ApplicationViewportInsetSupplier getBottomInsetProvider();

    ActivityTabProvider getActivityTabProvider();

    View getRootView();

    AssistantSnackbarFactory getSnackbarFactory();

    // Only called by native to guarantee future type safety.
    @CalledByNative
    default AssistantStaticDependencies getStaticDependencies() {
        return this;
    }
}
