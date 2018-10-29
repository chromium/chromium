// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.annotations.UsedByReflection;

/**
 * Class only compiled into android app bundles but not into APKs. Used to determine whether a build
 * is a bundle.
 */
@UsedByReflection("BundleUtils.java")
/* package */ class BundleCanary {}
