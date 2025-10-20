// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A class to represent a location in Chromium source code.
 *
 * <p>This is the Java equivalent of C++ {@code base::Location}, but is not directly convertible to
 * or from {@code base::Location} since {@code base::Location} expects pointers to static constant
 * strings which Java does not support.
 */
@NullMarked
public class Location {
    public final String fileName;
    public final String functionName;
    public final int lineNumber;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    Location(String fileName, String functionName, int lineNumber) {
        this.fileName = fileName;
        this.functionName = functionName;
        this.lineNumber = lineNumber;
    }

    public static @Nullable Location from(String fileName, String functionName, int lineNumber) {
        if (!(EarlyTraceEvent.enabled() || TraceEvent.enabled())) {
            return null;
        }

        return new Location(fileName, functionName, lineNumber);
    }

    @Override
    public boolean equals(Object obj) {
        return (obj instanceof Location other)
                && this.fileName.equals(other.fileName)
                && this.functionName.equals(other.functionName)
                && (this.lineNumber == other.lineNumber);
    }

    @Override
    public String toString() {
        return this.fileName + ":" + this.lineNumber + " " + this.functionName + "()";
    }
}
