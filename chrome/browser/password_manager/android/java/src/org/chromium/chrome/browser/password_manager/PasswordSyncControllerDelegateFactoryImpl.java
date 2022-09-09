// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * Instantiable version of {@link PasswordSyncControllerDelegateFactory}, don't add anything to this
 * class. Downstream provide an actual implementation. In GN, we specify that
 * {@link PasswordSyncControllerDelegate} is compiled separately from its implementation; other
 * projects may specify a different PasswordSyncControllerDelegateFactory via GN.
 */
class PasswordSyncControllerDelegateFactoryImpl extends PasswordSyncControllerDelegateFactory {}
