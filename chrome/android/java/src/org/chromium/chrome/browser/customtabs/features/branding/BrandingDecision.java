// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class used to indicate what branding decision needs to make for the embedded app.
 */
@Retention(RetentionPolicy.SOURCE)
@IntDef({
        BrandingDecision.NONE, BrandingDecision.TOOLBAR,
        // BrandingDecision.TOAST
})
@interface BrandingDecision {
    int NONE = 1;
    int TOOLBAR = 2;
    // TODO(crrev.com/c/3769601): Support toast branding.
    // int TOAST = 3; // Default.
}
