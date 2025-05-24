// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;

/** Resolves a tab, given a Tab ID. */
@FunctionalInterface
@NullMarked
public interface TabResolver {
    /**
     * @return {@link} Tab corresponding to a tab id
     */
    Tab resolve(int id);
}
