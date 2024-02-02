// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.List;

/** Helper class to handle color picker related utilities. */
public class ColorPickerUtils {
    /**
     * This method returns the color id list attributed to tab groups specifically.
     *
     * @return An array list of ids from 0 to n representing all colors in the palette
     */
    public static List<Integer> getTabGroupColorIdList() {
        // The color ids used here can be found in {@link TabGroupColorId}. Note that it is assumed
        // the id list is contiguous from 0 to size-1.
        List<Integer> colors = new ArrayList<>();
        for (int i = 0; i < TabGroupColorId.class.getDeclaredFields().length; i++) {
            colors.add(i);
        }
        return colors;
    }
}
