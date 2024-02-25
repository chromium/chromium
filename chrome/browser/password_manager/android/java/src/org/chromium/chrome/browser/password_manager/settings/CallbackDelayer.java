// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

/** This is an interface for delaying running of callbacks. */
public interface CallbackDelayer {
    /**
     * Run a callback after a delay specific to a particular implementation. The callback is always
     * run asynchronously.
     * @param callback The callback to be run.
     */
    void delay(Runnable callback);
}
