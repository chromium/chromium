// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.DialogPreference;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Launches the UI to edit, create or delete an Autofill profile entry. */
@NullMarked
public class AutofillProfileEditorPreference extends DialogPreference {

    public AutofillProfileEditorPreference(Context context) {
        this(context, null);
    }

    public AutofillProfileEditorPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        SettingsUtils.initializePreferenceDefaults(context, attrs, this);
    }

    /**
     * @return ID of the profile to edit when this preference is selected.
     */
    public @Nullable String getGUID() {
        return getExtras().getString(AutofillEditorBase.AUTOFILL_GUID);
    }
}
