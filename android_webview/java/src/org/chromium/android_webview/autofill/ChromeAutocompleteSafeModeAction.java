// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.autofill;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;

/**
 * A {@link SafeModeAction} to disable autocomplete provided by Chrome.
 *
 * This action itself is a NOOP. The actual work is done in
 * AwContents.isChromeAutocompleteSafeModeEnabled()
 */
@Lifetime.Singleton
public class ChromeAutocompleteSafeModeAction implements SafeModeAction {
    // This ID should not be changed or reused.
    public static final String ID = "disable_chrome_autocomplete";

    private static boolean sIsChromeAutocompleteDisabled;

    @Override
    @NonNull
    public String getId() {
        return ID;
    }

    @Override
    public boolean execute() {
        sIsChromeAutocompleteDisabled = true;
        return true;
    }

    public static boolean isChromeAutocompleteDisabled() {
        return sIsChromeAutocompleteDisabled;
    }
}
