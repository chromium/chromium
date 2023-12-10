// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** The annotated method or class will have -maximumremovedandroidloglevel 0 applied to it. */
@Target({ElementType.CONSTRUCTOR, ElementType.METHOD, ElementType.TYPE})
@Retention(RetentionPolicy.CLASS)
public @interface DoNotStripLogs {}
