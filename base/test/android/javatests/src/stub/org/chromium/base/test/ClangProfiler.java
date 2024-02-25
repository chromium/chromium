// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

/** Placeholder class used when clang profiling is not enabled at building. */
public class ClangProfiler {
    private ClangProfiler() {}

    public static void writeClangProfilingProfile() {}
}
