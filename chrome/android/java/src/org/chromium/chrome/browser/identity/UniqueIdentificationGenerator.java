// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity;

import androidx.annotation.Nullable;

/**
 * Interface used for uniquely identifying an installation of Chrome. To get an instance you should
 * use {@link UniqueIdentificationGeneratorFactory}.
 */
public interface UniqueIdentificationGenerator {
    /**
     * Creates a string that uniquely identifies this installation.
     * <p/>
     * If there is an error in generating the string, an empty string must be returned, not null.
     *
     * @param salt the salt to use for the unique ID.
     * @return a unique ID. On failure to generate, it must return the empty string.
     */
    String getUniqueId(@Nullable String salt);
}
