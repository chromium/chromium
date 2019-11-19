// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewStructure;

import org.chromium.base.annotations.VerifiesOnO;

/**
 * Sub-class of UrlBar that contains newer Android APIs to avoid verification errors.
 *
 * Only super calls to new Android APIs belong here - if it is a normal call to a new Android API,
 * use ApiHelperForX. See crbug.com/999165 for more description of what verification errors are and
 * why they are expensive.
 */
@VerifiesOnO
public class UrlBarApi26 extends UrlBar {
    public UrlBarApi26(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onProvideAutofillStructure(ViewStructure structure, int autofillFlags) {
        // https://crbug.com/996402: Prevent breaking autofill services on newer versions of
        // Android.
        mRequestingAutofillStructure = true;
        super.onProvideAutofillStructure(structure, autofillFlags);
        mRequestingAutofillStructure = false;
    }
}
