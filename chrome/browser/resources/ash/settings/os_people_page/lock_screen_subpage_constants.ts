// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const AllowedLocalAuthFactorsPref =
    'ash.local_auth_factors.enabled_factors';

/**
 * Possible values for `AllowedLocalAuthFactors`.
 */
export const AllowedLocalAuthFactors = {
  ALL: 'ALL',
  LOCAL_PASSWORD: 'LOCAL_PASSWORD',
  PIN: 'PIN',
};
