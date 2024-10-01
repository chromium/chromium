// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewStructure;
import android.widget.EditText;

/**
 * A wrapper class around {@link EditText} to stop Android Autofill from suggesting cards. This is
 * achieved by overriding the {@code onProvideAutofillStructure} method.
 */
public class EditTextNoAutofillView extends EditText {

    public EditTextNoAutofillView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onProvideAutofillStructure(ViewStructure structure, int flags) {}
}
