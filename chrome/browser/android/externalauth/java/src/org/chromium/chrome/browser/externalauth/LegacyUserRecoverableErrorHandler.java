// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalauth;

import android.content.Context;

import org.chromium.components.externalauth.UserRecoverableErrorHandler;

/**
 * Temporary class, identical to {@link UserRecoverableErrorHandler}, for migration.
 *
 * This class will be referenced temporarily while UserRecoverableErrorHandler is moved to
 * components.
 *
 * TODO(crbug.com/1144858): Remove this class after downstream uses
 * components.externalauth.UserRecoverableErrorHandler.
 */
public abstract class LegacyUserRecoverableErrorHandler extends UserRecoverableErrorHandler {
    public static final class Silent extends LegacyUserRecoverableErrorHandler {
        @Override
        protected final void handle(final Context context, final int errorCode) {}
    }
}
