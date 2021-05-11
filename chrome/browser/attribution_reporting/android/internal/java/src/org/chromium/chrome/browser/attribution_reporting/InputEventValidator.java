// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Build;
import android.view.InputEvent;

import org.chromium.base.ContextUtils;
import org.chromium.base.Predicate;
import org.chromium.base.compat.ApiHelperForR;

/**
 * Validates input events for Attribution Reporting, using InputManager#verifyInputEvent
 * on Android R+.
 */
public class InputEventValidator implements Predicate<InputEvent> {
    @Override
    public boolean test(InputEvent inputEvent) {
        // We cannot verify input events pre-R, so we're making a trade-off of compat vs. security
        // by allowing un-verified input pre-R.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return true;

        InputManager im = (InputManager) ContextUtils.getApplicationContext().getSystemService(
                Context.INPUT_SERVICE);

        // TODO(https://crbug.com/1198308): Ensure we aren't being sent duplicate or old Events.
        return ApiHelperForR.verifyInputEvent(im, inputEvent) != null;
    }
}
