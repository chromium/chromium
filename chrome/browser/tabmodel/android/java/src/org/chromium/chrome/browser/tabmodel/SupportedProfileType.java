// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** The type of tab/profile the activity supports. */
@NullMarked
@IntDef({
    SupportedProfileType.UNSET,
    SupportedProfileType.REGULAR,
    SupportedProfileType.OFF_THE_RECORD,
    SupportedProfileType.MIXED
})
@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.SOURCE)
public @interface SupportedProfileType {
    int UNSET = 0;
    int REGULAR = 1;
    int OFF_THE_RECORD = 2;
    int MIXED = 3;
}
