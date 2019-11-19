// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

/**
 * Use {@link #createComponent()} to instantiate a {@link ManualFillingComponent}.
 */
public class ManualFillingComponentFactory {
    private ManualFillingComponentFactory() {}

    /**
     * Creates a {@link ManualFillingCoordinator}.
     * @return A {@link ManualFillingCoordinator}.
     */
    public static ManualFillingComponent createComponent() {
        return new ManualFillingCoordinator();
    }
}
