// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Deployed version if CCA is deployed from cca.py deploy.
 *
 * This will be overridden by cca.py deploy with a simple string of deployed
 * timestamp, used only for developer to identify the deployed status.
 */
export const DEPLOYED_VERSION: string|undefined = undefined;
