// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

/**
 * A stub AuxiliarySearchHooks implementation. Replace at build time on internal builds.
 */
public class AuxiliarySearchHooksImpl implements AuxiliarySearchHooks {
    public static AuxiliarySearchHooks getInstance() {
        return new AuxiliarySearchHooksImpl();
    }

    public AuxiliarySearchHooksImpl() {}
}
