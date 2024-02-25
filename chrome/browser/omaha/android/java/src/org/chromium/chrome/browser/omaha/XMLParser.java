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

import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

/** Breaks XML down into its constituent elements and attributes. */
public class XMLParser extends DefaultHandler {
    static final class Node {
        public final String tag;
        public final Map<String, String> attributes;
        public final List<Node> children;

        public Node(String tagName) {
            tag = tagName;
            attributes = new HashMap<String, String>();
            children = new ArrayList<Node>();
        }
    }

    private final Node mRootNode;
    private final Stack<Node> mTagStack;

    public XMLParser(String serverResponse) throws RequestFailureException {
        mRootNode = new Node(null);
        mTagStack = new Stack<Node>();
        mTagStack.push(mRootNode);

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

        if (mTagStack.peek() != mRootNode) {
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
        if (mTagStack.empty()) throw new SAXException("Tag stack is empty when it shouldn't be.");

        Node currentNode = new Node(qName);
        mTagStack.peek().children.add(currentNode);
        mTagStack.push(currentNode);

        for (int i = 0; i < attributes.getLength(); ++i) {
            String attributeName = attributes.getLocalName(i);
            String attributeValue = attributes.getValue(attributeName);
            currentNode.attributes.put(attributeName, attributeValue);
        }
    }

    @Override
    public void endElement(String uri, String localName, String qName) throws SAXException {
        if (mTagStack.empty()) {
            throw new SAXException("Tried closing empty stack with " + qName);
        } else if (!TextUtils.equals(qName, mTagStack.peek().tag)) {
            throw new SAXException("Tried closing " + mTagStack.peek().tag + " with " + qName);
        }
        mTagStack.pop();
    }
}
