// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * Instantiable version of {@link PasswordCheckupClientHelperFactory}, don't add anything to this
 * class. Downstream provides an actual implementation. In the build files, we specify that
 * {@link PasswordCheckupClientHelperFactory} is compiled separately from its implementation; other
 * projects may specify a different PasswordCheckupClientHelperFactoryImpl via GN.
 */
class PasswordCheckupClientHelperFactoryImpl extends PasswordCheckupClientHelperFactory {}
