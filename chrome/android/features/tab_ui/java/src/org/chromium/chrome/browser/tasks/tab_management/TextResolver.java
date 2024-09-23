// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.Nullable;

/** Resolver of dynamic text. */
@FunctionalInterface
public interface TextResolver {
    /**
     * @param context The context to use for resolving the text.
     * @return the character sequence to show.
     */
    @Nullable
    CharSequence resolve(Context context);
}
