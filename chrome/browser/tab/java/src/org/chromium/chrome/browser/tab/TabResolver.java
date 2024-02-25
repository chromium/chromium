// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

/** Resolves a tab, given a Tab ID. */
@FunctionalInterface
public interface TabResolver {
    /**
     * @return {@link} Tab corresponding to a tab id
     */
    Tab resolve(int id);
}
