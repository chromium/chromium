// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

/** Fake implementation of Credential Manager Exception. */
public final class FakeAndroidCredManException extends Exception {
    private final String mType;

    public FakeAndroidCredManException(String type, String message) {
        super(message, null);
        mType = type;
    }

    public String getType() {
        return mType;
    }
}
