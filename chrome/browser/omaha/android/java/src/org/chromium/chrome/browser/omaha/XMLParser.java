// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.text.TextUtils;

import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.helpers.DefaultHandler;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

/** Breaks XML down into its constituent elements and attributes. */
@NullMarked
public class XMLParser extends DefaultHandler {
    static final class Node {
        public final @Nullable String tag;
        public final Map<String, String> attributes;
        public final List<Node> children;

        public Node(@Nullable String tagName) {
            tag = tagName;
            attributes = new HashMap<>();
            children = new ArrayList<>();
        }
    }

    private final Node mRootNode;
    private final Deque<Node> mTagStack;

    public XMLParser(String serverResponse) throws RequestFailureException {
        mRootNode = new Node(null);
        mTagStack = new ArrayDeque<>();
        mTagStack.addLast(mRootNode);

        try {
            SAXParserFactory factory = SAXParserFactory.newInstance();
            SAXParser saxParser = factory.newSAXParser();
            saxParser.parse(new InputSource(new StringReader(serverResponse)), this);
        } catch (IOException e) {
            throw new RequestFailureException(
                    "Hit IOException", e, RequestFailureException.ERROR_MALFORMED_XML);
        } catch (ParserConfigurationException e) {
            throw new RequestFailureException(
                    "Hit ParserConfigurationException",
                    e,
                    RequestFailureException.ERROR_MALFORMED_XML);
        } catch (SAXParseException e) {
            throw new RequestFailureException(
                    "Hit SAXParseException", e, RequestFailureException.ERROR_MALFORMED_XML);
        } catch (SAXException e) {
            throw new RequestFailureException(
                    "Hit SAXException", e, RequestFailureException.ERROR_MALFORMED_XML);
        }

        if (mTagStack.peekLast() != mRootNode) {
            throw new RequestFailureException(
                    "XML was malformed.", RequestFailureException.ERROR_MALFORMED_XML);
        }
    }

    public Node getRootNode() {
        return mRootNode;
    }

    @Override
    public void startElement(String uri, String localName, String qName, Attributes attributes)
            throws SAXException {
        if (mTagStack.isEmpty()) {
            throw new SAXException("Tag stack is empty when it shouldn't be.");
        }

        Node currentNode = new Node(qName);
        mTagStack.peekLast().children.add(currentNode);
        mTagStack.addLast(currentNode);

        for (int i = 0; i < attributes.getLength(); ++i) {
            String attributeName = attributes.getLocalName(i);
            String attributeValue = attributes.getValue(attributeName);
            currentNode.attributes.put(attributeName, attributeValue);
        }
    }

    @Override
    public void endElement(String uri, String localName, String qName) throws SAXException {
        if (mTagStack.isEmpty()) {
            throw new SAXException("Tried closing empty stack with " + qName);
        } else if (!TextUtils.equals(qName, mTagStack.peekLast().tag)) {
            throw new SAXException("Tried closing " + mTagStack.peekLast().tag + " with " + qName);
        }
        mTagStack.removeLast();
    }
}
