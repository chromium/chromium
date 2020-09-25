// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import java.util.Locale;
import java.util.regex.PatternSyntaxException;

/**
 * Sanitizes Strings sent to the Omaha server.
 */
public class StringSanitizer {
    static final char[] CHARS_TO_REMOVE = {';', ',', '"', '\'', '\n', '\r', '\t'};

    public static String sanitize(String str) {
        for (char current : CHARS_TO_REMOVE) {
            str = str.replace(current, ' ');
        }
        try {
            str = str.replaceAll("  *", " ");
        } catch (PatternSyntaxException e) {
            assert false;
        }
        str = str.toLowerCase(Locale.US);
        str = str.trim();
        return str;
    }
}
