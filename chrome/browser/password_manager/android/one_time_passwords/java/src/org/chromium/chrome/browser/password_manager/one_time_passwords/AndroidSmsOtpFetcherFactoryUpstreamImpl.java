// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager.one_time_passwords;

import org.chromium.build.annotations.NullMarked;

/**
 * Instantiable version of {@link AndroidSmsOtpFetcherFactory}, don't add anything to this class.
 * Downstream provides an actual implementation via ServiceLoader/@ServiceImpl.
 */
@NullMarked
class AndroidSmsOtpFetcherFactoryUpstreamImpl extends AndroidSmsOtpFetcherFactory {}
