// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.keyboard;

/**
 * Miscellaneous build-related constants.
 */
public class BuildConstants {
    /**
     * The version number of the keyboard API. A local copy of this class should
     * be built into both the client and the SDK. Then at SDK load time, the
     * version numbers can be compared to make sure the client and SDK have
     * compatible APIs.
     */
    public static final long API_VERSION = 1;
}
