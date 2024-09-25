// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * Instantiable version of {@link PasswordManagerBackendSupportHelper}, don't add anything to this
 * class. Downstream provides an actual implementation via ServiceLoader/@ServiceImpl.
 */
class PasswordManagerBackendSupportHelperUpstreamImpl extends PasswordManagerBackendSupportHelper {}
