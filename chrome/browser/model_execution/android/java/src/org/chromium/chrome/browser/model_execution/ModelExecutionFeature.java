// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.model_execution;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
    ModelExecutionFeature.PAGE_INFO,
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface ModelExecutionFeature {
    public static int PAGE_INFO = 1;
}
