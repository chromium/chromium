// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes.fonts;

import org.chromium.components.content_creation.notes.models.TextStyle;

import java.util.Locale;
import java.util.Objects;

/** Class used to represent queries used to load fonts. */
public class TypefaceRequest {
    private static final String CONDENSED_SUBSTRING = " Condensed";
    private static final int CONDENSED_WIDTH_VALUE = 75;

    public final String fontName;
    public final int weight;

    public static TypefaceRequest createFromTextStyle(TextStyle textStyle) {
        return new TypefaceRequest(textStyle.fontName, textStyle.weight);
    }

    public TypefaceRequest(String fontName, int weight) {
        this.fontName = fontName;
        this.weight = weight;
    }

    /**
     * Returns a string containing a query used to load fonts.
     * The query format is provided by the official Google Fonts documentation:
     * https://developers.google.com/fonts/docs/android#query_format
     */
    public String toQuery() {
        int condensedIndex = this.fontName.lastIndexOf(CONDENSED_SUBSTRING);
        if (condensedIndex != -1) {
            // Build a query for the condensed version of the font. Condensed
            // fonts are simply fonts with a width of 75. For example,
            // "Roboto Condensed" is "Roboto" with a width of 75.
            String actualFontName = this.fontName.substring(0, condensedIndex);
            return String.format(
                    (Locale) null,
                    "name=%s&weight=%d&width=%d",
                    actualFontName,
                    this.weight,
                    CONDENSED_WIDTH_VALUE);
        }

        return String.format((Locale) null, "name=%s&weight=%d", this.fontName, this.weight);
    }

    @Override
    public boolean equals(Object other) {
        if (other == this) {
            return true;
        }

        if (!(other instanceof TypefaceRequest)) {
            return false;
        }

        TypefaceRequest otherRequest = (TypefaceRequest) other;

        // If the queries are the same, then the requests are effectively the
        // same as well.
        return toQuery().equalsIgnoreCase(otherRequest.toQuery());
    }

    @Override
    public int hashCode() {
        return Objects.hash(toQuery());
    }
}
