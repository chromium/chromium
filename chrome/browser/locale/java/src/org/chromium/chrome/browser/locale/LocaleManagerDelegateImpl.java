// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

/**
 * Instantiable version of {@link LocaleManagerDelegate}, don't add anything to this class.
 * Downstream targets may provide a different implementation. In GN, we specify that
 * {@link LocaleManagerDelegate} is compiled separately from its implementation; other
 * projects may specify a different LocaleManagerDelegate via GN.
 */
class LocaleManagerDelegateImpl extends LocaleManagerDelegate {}
