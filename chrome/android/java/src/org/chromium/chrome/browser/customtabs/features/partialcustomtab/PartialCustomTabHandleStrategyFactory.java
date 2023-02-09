// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.content.Context;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;

import java.util.function.BooleanSupplier;

/**
 * The factory implementation for creating Partial Custom Tab handle strategies that will be applied
 * to partial custom tabs for which resizing by dragging is supported.
 */
public class PartialCustomTabHandleStrategyFactory {
    public PartialCustomTabHandleStrategy create(@PartialCustomTabType int type, Context context,
            BooleanSupplier isFullHeight, Supplier<Integer> status,
            PartialCustomTabHandleStrategy.DragEventCallback dragEventCallback) {
        switch (type) {
            case PartialCustomTabType.BOTTOM_SHEET: {
                return new PartialCustomTabHandleStrategy(
                        context, isFullHeight, status, dragEventCallback);
            }
            case PartialCustomTabType.SIDE_SHEET:
            case PartialCustomTabType.FULL_SIZE: {
                return null;
            }
            default: {
                assert false : "Partial Custom Tab type not supported: " + type;
            }
        }

        return null;
    }
}
