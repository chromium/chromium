// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalauth;

import org.chromium.components.externalauth.ExternalAuthUtils;

/**
 * Temporary class, identical to {@link ExternalAuthUtils}, for migration.
 *
 * This class will be referenced temporarily while ExternalAuthUtils is moved to components.
 *
 * TODO(crbug.com/1144858): Remove this class after downstream uses
 * components.externalauth.ExternalAuthUtils.
 */
public class LegacyExternalAuthUtils extends ExternalAuthUtils {
    public boolean canUseGooglePlayServices(final LegacyUserRecoverableErrorHandler errorHandler) {
        return super.canUseGooglePlayServices(errorHandler);
    }

    public boolean canUseFirstPartyGooglePlayServices(
            LegacyUserRecoverableErrorHandler userRecoverableErrorHandler) {
        return super.canUseFirstPartyGooglePlayServices(userRecoverableErrorHandler);
    }
}
