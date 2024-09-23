// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

/**
 * Qualifiers that specify which particular instance of given type is provided or injected, in cases
 * when it is ambiguous.
 */
public interface ChromeCommonQualifiers {
    String ACTIVITY_CONTEXT = "ACTIVITY_CONTEXT";
    String APP_CONTEXT = "APP_CONTEXT";

    String SAVED_INSTANCE_SUPPLIER = "SAVED_INSTANCE_SUPPLIER";
    String ACTIVITY_TYPE = "ACTIVITY_TYPE";
}
