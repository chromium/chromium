// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import static java.lang.annotation.ElementType.TYPE_USE;
import static java.lang.annotation.RetentionPolicy.SOURCE;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/**
 * Annotation to mark the id of a window. This is currently being used to index different windows
 * and tab models, but most clients should simply treat it as an opaque identifier.
 */
@Retention(SOURCE)
@Target(TYPE_USE)
@NullMarked
public @interface WindowId {}
