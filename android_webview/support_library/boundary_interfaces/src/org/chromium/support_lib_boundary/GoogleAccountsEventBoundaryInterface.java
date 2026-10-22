// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Boundary interface for GoogleAccountsEvent. */
@NullMarked
public interface GoogleAccountsEventBoundaryInterface {
    @IntDef({
        ACTION_TYPE_OTHER,
        ACTION_TYPE_SIGN_OUT,
        ACTION_TYPE_CREATE_ACCOUNT,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ActionType {}

    /** Any other action type. */
    public static final int ACTION_TYPE_OTHER = 0;

    public static final int ACTION_TYPE_SIGN_OUT = 1;
    public static final int ACTION_TYPE_CREATE_ACCOUNT = 2;

    @ActionType
    int getActionType();

    String getContinueUrl();

    boolean isOutermostMainFrame();

    boolean isNavigationRequest();
}
