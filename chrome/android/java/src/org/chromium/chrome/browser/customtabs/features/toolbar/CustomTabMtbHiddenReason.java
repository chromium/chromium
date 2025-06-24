// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Enums for the reason why the adaptive toolbar button on CCT is hidden. */
// LINT.IfChange(CustomTabMtbHiddenReason)
@IntDef({
    CustomTabMtbHiddenReason.OTHER_REASON,
    CustomTabMtbHiddenReason.INVALID_VARIANT,
    CustomTabMtbHiddenReason.DUPLICATED_ACTION,
    CustomTabMtbHiddenReason.CPA_ONLY_MODE,
    CustomTabMtbHiddenReason.NO_BUTTON_SPACE,
    CustomTabMtbHiddenReason.TOOLBAR_WIDTH_LIMIT,
    CustomTabMtbHiddenReason.OMNIBOX_ENABLED,
    CustomTabMtbHiddenReason.COUNT,
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface CustomTabMtbHiddenReason {
    int OTHER_REASON = 0;
    int INVALID_VARIANT = 1;
    int DUPLICATED_ACTION = 2;
    int CPA_ONLY_MODE = 3;
    int NO_BUTTON_SPACE = 4;
    int TOOLBAR_WIDTH_LIMIT = 5;
    int OMNIBOX_ENABLED = 6;
    int COUNT = 7;
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/custom_tabs/enums.xml:CustomTabsMtbHiddenReason)
