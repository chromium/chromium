// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Add this annotation to tests so that Chrome pretends the VR module is not installed. */
@Target({ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
public @interface VrModuleNotInstalled {}
