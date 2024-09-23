// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

/**
 * A string searching algorithm that finds the index of the first match of a substring within a
 * string. Preprocesses the substring with an offset table to skip sections of the text.
 */
public class BoyerMoore {
    /**
     * Returns the index within the haystack of the first occurrence of the needle. Returns -1 if no
     * match found.
     *
     * @param haystack The string to be scanned
     * @param needle The target string to search
     * @return The start index of the substring
     */
    public static int indexOf(char[] haystack, char[] needle) {
        if (needle.length == 0) {
            return 0;
        }
        int[] offsetTable = makeOffsetTable(needle);
        int m = needle.length;
        int n = haystack.length;

        int skip;
        int stop = n - m;

        for (int i = 0; i <= stop; i += skip) {
            skip = 0;
            for (int j = m - 1; j >= 0; --j) {
                char ch = haystack[i + j];
                if (needle[j] != ch
                        && !(Character.isWhitespace(needle[j]) && Character.isWhitespace(ch))) {
                    int k = j - (ch >= offsetTable.length ? -1 : offsetTable[ch]);
                    skip = k > 1 ? k : 1;
                    break;
                }
            }
            // Found.
            if (skip == 0) {
                return i;
            }
        }
        // Not found.
        return -1;
    }

    private static int[] makeOffsetTable(char[] needle) {
        int max = 0;
        for (char c : needle) {
            if (c > max) {
                max = c;
            }
        }
        int[] offsetTable = new int[++max];
        // Position of rightmost occurrence of c in the needle.
        for (int c = 0; c < max; ++c) {
            offsetTable[c] = -1;
        }
        int len = needle.length;
        for (int j = 0; j < len; ++j) {
            offsetTable[needle[j]] = j;
        }
        return offsetTable;
    }
}
