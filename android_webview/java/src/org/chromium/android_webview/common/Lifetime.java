// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Annotations specifying the lifetimes of classes in embedded WebView code.
 *
 * See: //android_webview/docs/lifetime-annotations.md.
 */
@NullMarked
public class Lifetime {
    private Lifetime() {}

    @Retention(RetentionPolicy.SOURCE)
    public @interface Singleton {}

    @Retention(RetentionPolicy.SOURCE)
    public @interface Profile {}

    @Retention(RetentionPolicy.SOURCE)
    public @interface WebView {}

    @Retention(RetentionPolicy.SOURCE)
    public @interface Temporary {}

    @Retention(RetentionPolicy.SOURCE)
    public @interface Renderer {}
}
