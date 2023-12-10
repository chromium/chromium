// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.Supplier;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Defines the policy to determine the next tab after a tab is closed. */
@Retention(RetentionPolicy.SOURCE)
@IntDef({NextTabPolicy.HIERARCHICAL, NextTabPolicy.LOCATIONAL})
public @interface NextTabPolicy {
    /** Prefer to show a parent tab next. */
    int HIERARCHICAL = 0;

    /** Prefer to show an adjacent tab next. */
    int LOCATIONAL = 1;

    /** Supplier for {@link NextTabPolicy}. */
    interface NextTabPolicySupplier extends Supplier</*@NextTabPolicy*/ Integer> {}
}
