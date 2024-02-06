// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

/**
 * The delegate responsible for handling UI-specific arrangements on each {@link ColorPicker}
 * implementation.
 */
public interface ColorPickerDelegate {
    /** Retrieve the UI component used for inflating the color picker. */
    int getColorPickerUIComponent();
}
