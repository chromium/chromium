// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

/**
 * Interface to be implemented by *ArTestRule rules.
 *
 * Does not currently actually do anything other than make it possible to check that a given rule
 * is AR-specific.
 */
public interface ArTestRule extends XrTestRule {}
