// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/** Implemented by entities that may host incognito tabs. */
public interface IncognitoTabHost {
    /** Whether has any incognito tabs at the moment. */
    boolean hasIncognitoTabs();

    /** Close all incognito tabs. */
    void closeAllIncognitoTabs();

    /** Whether there is any active incognito session at the moment.*/
    boolean isActiveModel();
}
