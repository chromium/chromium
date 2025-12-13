// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * This is Application class for Monochrome.
 *
 * <p>You shouldn't add anything else in this file, this class is split off from normal chrome in
 * order to access Android system API through Android WebView glue layer and have monochrome
 * specific code.
 *
 * <p>This class is NOT used by Trichrome. Do not add anything here which is only related to
 * Monochrome's minimum SDK level or APK packaging decisions, because those are likely to apply to
 * Trichrome as well - this must only be used for things specific to functioning as a WebView
 * implementation.
 */
public class MonochromeApplicationImpl extends ChromeApplicationImpl {
    public MonochromeApplicationImpl() {}
}
