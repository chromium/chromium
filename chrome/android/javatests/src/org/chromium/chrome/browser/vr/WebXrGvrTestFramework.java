// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.junit.Assert;

import org.chromium.chrome.test.ChromeActivityTestRule;

/** Extension of WebXrGvrTestFramework containing WebXR for Gvr-specific functionality. */
public class WebXrGvrTestFramework extends WebXrVrTestFramework {
    public WebXrGvrTestFramework(ChromeActivityTestRule rule) {
        super(rule);
        Assert.assertFalse("Test started in VR", VrShellDelegate.isInVr());
    }
}
