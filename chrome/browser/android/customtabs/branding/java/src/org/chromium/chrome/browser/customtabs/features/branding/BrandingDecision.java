// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Class used to indicate what branding decision needs to make for the embedded app. */
@Retention(RetentionPolicy.SOURCE)
@IntDef({
    BrandingDecision.NONE,
    BrandingDecision.TOOLBAR,
    BrandingDecision.TOAST,
    BrandingDecision.NUM_ENTRIES
})
@interface BrandingDecision {
    int NONE = 0;
    int TOOLBAR = 1;
    int TOAST = 2;

    int NUM_ENTRIES = 3;
}
