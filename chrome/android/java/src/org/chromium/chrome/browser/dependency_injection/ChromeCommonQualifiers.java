// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

/**
 * Qualifiers that specify which particular instance of given type is provided or injected, in cases
 * when it is ambiguous.
 */
public interface ChromeCommonQualifiers {
    String LAST_USED_PROFILE = "LAST_USED_PROFILE";

    String ACTIVITY_CONTEXT = "ACTIVITY_CONTEXT";
    String APP_CONTEXT = "APP_CONTEXT";
}
