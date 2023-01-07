// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import android.view.View;

/**
 * Constants for visibility of a view.
 * <p>
 * Maps to View.VISIBLE, View.INVISIBLE, and View.GONE.
 */
public enum ViewVisibility {
    VISIBLE,
    INVISIBLE,
    GONE;

    static int toVisibility(ViewVisibility v) {
        switch (v) {
            case VISIBLE:
                return View.VISIBLE;
            case INVISIBLE:
                return View.INVISIBLE;
            case GONE:
                return View.GONE;
        }
        return View.GONE;
    }
}
