// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.helpers.DefaultHandler;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.io.StringReader;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

/**
 * Pulls out a given tag attribute's value from the XML. Assumes that the same tag doesn't appear
 * twice with the same attribute.
 */
@NullMarked
public class AttributeFinder extends DefaultHandler {
    private static final String TAG = "AttributeFinder";

    private final String mDesiredTag;
    private final @Nullable String mDesiredAttribute;
    private boolean mTagFound;
    private @Nullable String mValue;

    public AttributeFinder(String xml, String tag, @Nullable String attribute) {
        mDesiredTag = tag;
        mDesiredAttribute = attribute;

        try {
            SAXParserFactory factory = SAXParserFactory.newInstance();
            SAXParser saxParser = factory.newSAXParser();
            saxParser.parse(new InputSource(new StringReader(xml)), this);
        } catch (IOException e) {
            Log.e(TAG, "Hit IOException", e);
        } catch (ParserConfigurationException e) {
            Log.e(TAG, "Hit ParserConfigurationException", e);
        } catch (SAXParseException e) {
            Log.e(TAG, "Hit SAXParseException", e);
        } catch (SAXException e) {
            Log.e(TAG, "Hit SAXException", e);
        }
    }

    @Override
    public void startElement(String uri, String localName, String tag, Attributes attributes) {
        if (tag.equals(mDesiredTag)) {
            mTagFound = true;
            mValue = mDesiredAttribute != null ? attributes.getValue(mDesiredAttribute) : null;
        }
    }

    public boolean isTagFound() {
        return mTagFound;
    }

    public @Nullable String getValue() {
        return mValue;
    }
}
