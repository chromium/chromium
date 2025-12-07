// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import org.chromium.build.annotations.NullMarked;

/** Delegate for cloud management functions implemented downstream for Google Chrome. */
@NullMarked
public interface CloudManagementAndroidConnectionDelegate {
    /** Returns the value of Gservices Android ID. */
    String getGservicesAndroidId();
}
