// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

/**
 * Boundary interface for SafeBrowsingResponseCompat.
 */
public interface SafeBrowsingResponseBoundaryInterface {
    void showInterstitial(boolean allowReporting);
    void proceed(boolean report);
    void backToSafety(boolean report);
}
