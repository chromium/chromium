// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;

import java.util.function.BooleanSupplier;

/**
 * The factory implementation for creating Partial Custom Tab handle strategies that will be applied
 * to partial custom tabs for which resizing by dragging is supported.
 */
public class PartialCustomTabHandleStrategyFactory {
    public CustomTabToolbar.HandleStrategy create(@PartialCustomTabType int type, Context context,
            BooleanSupplier isFullHeight, Supplier<Integer> status,
            PartialCustomTabHandleStrategy.DragEventCallback dragEventCallback,
            Callback<Runnable> closeAnimation) {
        switch (type) {
            case PartialCustomTabType.BOTTOM_SHEET: {
                return new PartialCustomTabHandleStrategy(
                        context, isFullHeight, status, dragEventCallback, closeAnimation);
            }
            case PartialCustomTabType.SIDE_SHEET:
            case PartialCustomTabType.FULL_SIZE: {
                return new SimpleHandleStrategy(closeAnimation);
            }
            default: {
                assert false : "Partial Custom Tab type not supported: " + type;
            }
        }

        return null;
    }
}
