// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

/**
 * An interface to control the interactability of elements in the tab-switcher toolbar.
 */
public interface IncognitoReauthTopToolbarDelegate {
    int disableNewTabButton();
    void enableNewTabButton(int clientToken);
}
