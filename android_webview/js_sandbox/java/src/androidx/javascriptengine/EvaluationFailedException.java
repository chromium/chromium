// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

import androidx.annotation.NonNull;

/** Wrapper for the exception thrown by the JS evaluation engine. */
public final class EvaluationFailedException extends JavaScriptException {
    public EvaluationFailedException(@NonNull String error) {
        super(error);
    }
}
