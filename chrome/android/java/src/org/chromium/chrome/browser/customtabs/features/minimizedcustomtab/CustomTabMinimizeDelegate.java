// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

/** Delegate for minimizing the Custom Tab. */
public interface CustomTabMinimizeDelegate {
    /** Minimize the Custom Tab into picture-in-picture. */
    void minimize();
}
