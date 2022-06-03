// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import androidx.preference.DialogPreference;

/**
 * Launches the UI to edit, create or delete an Autofill profile entry.
 */
public class AutofillProfileEditorPreference extends DialogPreference {
    public AutofillProfileEditorPreference(Context context) {
        super(context);
    }

    /**
     * @return ID of the profile to edit when this preference is selected.
     */
    public String getGUID() {
        return getExtras().getString(AutofillEditorBase.AUTOFILL_GUID);
    }
}
