// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * Instantiable version of {@link CredentialManagerLauncherFactory}, don't add anything to this
 * class. Downstream provides an actual implementation via ServiceLoader/@ServiceImpl.
 */
class CredentialManagerLauncherFactoryUpstreamImpl extends CredentialManagerLauncherFactory {}
