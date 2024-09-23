// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewStructure;
import android.widget.AutoCompleteTextView;

/**
 * A wrapper class around {@link AutoCompleteTextView} to stop Android Autofill from showing the
 * save password prompt. This is achieved by overriding the {@code onProvideAutofillStructure}
 * method.
 */
public class AutoCompleteTextViewNoAutofill extends AutoCompleteTextView {
    public AutoCompleteTextViewNoAutofill(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onProvideAutofillStructure(ViewStructure structure, int flags) {}
}
