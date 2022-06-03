// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * An annotation for specifying which playback file ArCore should use for the test.
 *
 * The specified path must be relative to the Chromium src directory, typically in
 * //chrome/test/data/xr/ar_playback_datasets/.
 */
@Target({ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
public @interface ArPlaybackFile {
    /**
     * @return The playback file to use.
     */
    public String value();
}
