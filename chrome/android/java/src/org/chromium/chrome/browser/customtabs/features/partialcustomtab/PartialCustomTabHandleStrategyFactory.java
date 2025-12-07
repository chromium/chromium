// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * The factory implementation for creating Partial Custom Tab handle strategies that will be applied
 * to partial custom tabs for which resizing by dragging is supported.
 */
@NullMarked
public class PartialCustomTabHandleStrategyFactory {
    public CustomTabToolbar.@Nullable HandleStrategy create(
            @PartialCustomTabType int type,
            Context context,
            BooleanSupplier isFullHeight,
            Supplier<Integer> status,
            PartialCustomTabHandleStrategy.DragEventCallback dragEventCallback) {
        return switch (type) {
            case PartialCustomTabType.BOTTOM_SHEET -> new PartialCustomTabHandleStrategy(
                    context, isFullHeight, status, dragEventCallback);
            case PartialCustomTabType.SIDE_SHEET, PartialCustomTabType.FULL_SIZE -> null;
            default -> {
                assert false : "Partial Custom Tab type not supported: " + type;
                yield null;
            }
        };
    }
}
