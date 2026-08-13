// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * IntDef for tri-state boolean values (NOT_SET, FALSE, TRUE) to replace @Nullable Boolean values.
 */
@NullMarked
@Retention(RetentionPolicy.SOURCE)
@IntDef({TriState.NOT_SET, TriState.FALSE, TriState.TRUE})
@Target(ElementType.TYPE_USE)
public @interface TriState {
    int NOT_SET = 0;
    int FALSE = 1;
    int TRUE = 2;
}
