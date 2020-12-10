// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalauth;

import android.content.Context;

import org.chromium.components.externalauth.VerifiedHandler;

/**
 * Temporary class, identical to {@link VerifiedHandler}, for migration.
 *
 * This class will be referenced temporarily while VerifiedHandler is moved to components.
 *
 * TODO(crbug.com/1144858): Remove this class after downstream uses
 * components.externalauth.VerifiedHandler.
 */
public class LegacyVerifiedHandler extends VerifiedHandler {
    public LegacyVerifiedHandler(
            Context context, LegacyExternalAuthUtils externalAuthUtils, int authRequirements) {
        super(context, externalAuthUtils, authRequirements);
    }
}
