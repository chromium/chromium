// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.autofill;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;

/**
 * A {@link SafeModeAction} to disable autocomplete provided by Chrome.
 *
 * This action itself is a NOOP. The actual work is done in
 * AwContents.isChromeAutocompleteSafeModeEnabled()
 */
@Lifetime.Singleton
public class ChromeAutocompleteSafeModeAction implements SafeModeAction {
    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.DISABLE_CHROME_AUTOCOMPLETE;

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
