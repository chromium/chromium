// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/** Implemented by entities that may host incognito tabs. */
@NullMarked
public interface IncognitoTabHost {
    /** Whether has any incognito tabs at the moment. */
    boolean hasIncognitoTabs();

    /** Close all incognito tabs. */
    void closeAllIncognitoTabs();

    /** An async version of {@link #closeAllIncognitoTabs()}, that will wait for init to finish. */
    void closeAllIncognitoTabsOnInit();

    /** Whether there is any active incognito session at the moment. */
    boolean isActiveModel();
}
