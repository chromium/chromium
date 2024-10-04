// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

/** Glue type to combine {@link IncognitoTabModel} and {@link TabModelInternal} interfaces. */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public interface IncognitoTabModelInternal extends IncognitoTabModel, TabModelInternal {}
