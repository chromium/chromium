// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.empty;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The properties required to build a {@link EmptyView}. */
interface EmptyProperties {
    @IntDef({State.LOADING, State.EMPTY, State.GONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        int LOADING = 0;
        int EMPTY = 1;
        int GONE = 2;
    }

    /** The current state of the empty view. */
    public static final WritableIntPropertyKey STATE = new WritableIntPropertyKey();

    /** The current text resource to use for the empty view. */
    public static final WritableIntPropertyKey EMPTY_TEXT_RES_ID = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {STATE, EMPTY_TEXT_RES_ID};
}