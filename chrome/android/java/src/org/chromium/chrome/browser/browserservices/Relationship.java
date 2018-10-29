// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

/**
 * This is a plain-old-data class to store a Digital Asset Link relationship [1].
 *
 * [1] https://developers.google.com/digital-asset-links/v1/getting-started
 */
public class Relationship {
    public final String packageName;
    public final Origin origin;
    public final int relation;

    /** Creates a {@link Relationship} to hold relationship details. */
    public Relationship(String packageName, Origin origin, int relation) {
        this.packageName = packageName;
        this.origin = origin;
        this.relation = relation;
    }

    /**
     * Serializes the Relationship to a String. This is used when storing relationships in
     * AndroidPreferences, so needs to be stable.
     */
    @Override
    public String toString() {
        // Neither package names nor origins contain commas.
        return packageName + "," + origin + "," + relation;
    }
}
