// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.text.TextUtils;
import android.util.Log;

import org.chromium.chrome.browser.omaha.OmahaBase.VersionConfig;
import org.chromium.chrome.browser.omaha.XMLParser.Node;

/**
 * Parses XML responses from the Omaha Update Server.
 *
 * Expects XML formatted like:
 * <?xml version="1.0" encoding="UTF-8"?>
 *   <daystart elapsed_days="4804" elapsed_seconds="65524"/>
 *   <app appid="{appid}" status="ok">
 *     <updatecheck status="ok">
 *       <urls>
 *         <url codebase="https://market.android.com/details?id=com.google.android.apps.chrome/"/>
 *       </urls>
 *       <manifest version="0.16.4130.199">
 *         <packages>
 *           <package hash="0" name="placeholder.apk" required="true" size="0"/>
 *         </packages>
 *         <actions>
 *           <action event="install" run="placeholder.apk"/>
 *           <action event="postinstall"/>
 *         </actions>
 *       </manifest>
 *     </updatecheck>
 *     <ping status="ok"/>
 *   </app>
 * </response>
 *
 * The appid is dependent on the variant of Chrome that is running.
 */
public class ResponseParser {
    private static final String TAG = "ResponseParser";

    // Tags that we care to parse from the response.
    private static final String TAG_APP = "app";
    private static final String TAG_DAYSTART = "daystart";
    private static final String TAG_EVENT = "event";
    private static final String TAG_MANIFEST = "manifest";
    private static final String TAG_PING = "ping";
    private static final String TAG_RESPONSE = "response";
    private static final String TAG_UPDATECHECK = "updatecheck";
    private static final String TAG_URL = "url";
    private static final String TAG_URLS = "urls";

    private final String mAppId;
    private final boolean mExpectInstallEvent;
    private final boolean mExpectPing;
    private final boolean mExpectUpdatecheck;
    private final boolean mStrictParsingMode;

    private Integer mDaystartSeconds;
    private Integer mDaystartDays;
    private String mAppStatus;

    private String mUpdateStatus;
    private String mNewVersion;
    private String mUrl;

    private boolean mParsedInstallEvent;
    private boolean mParsedPing;
    private boolean mParsedUpdatecheck;

    public ResponseParser(String appId, boolean expectInstallEvent) {
        this(appId, expectInstallEvent, !expectInstallEvent, !expectInstallEvent);
    }

    public ResponseParser(
            String appId,
            boolean expectInstallEvent,
            boolean expectPing,
            boolean expectUpdatecheck) {
        this(false, appId, expectInstallEvent, expectPing, expectUpdatecheck);
    }

    public ResponseParser(
            boolean strictParsing,
            String appId,
            boolean expectInstallEvent,
            boolean expectPing,
            boolean expectUpdatecheck) {
        mStrictParsingMode = strictParsing;
        mAppId = appId;
        mExpectInstallEvent = expectInstallEvent;
        mExpectPing = expectPing;
        mExpectUpdatecheck = expectUpdatecheck;
    }

    public VersionConfig parseResponse(String xml) throws RequestFailureException {
        XMLParser parser = new XMLParser(xml);
        Node rootNode = parser.getRootNode();
        parseRootNode(rootNode);
        return new VersionConfig(getNewVersion(), getURL(), getDaystartDays(), getUpdateStatus());
    }

    public int getDaystartSeconds() {
        if (mDaystartSeconds == null) return 0;
        return mDaystartSeconds;
    }

    public int getDaystartDays() {
        if (mDaystartDays == null) return 0;
        return mDaystartDays;
    }

    public String getNewVersion() {
        return mNewVersion;
    }

    public String getURL() {
        return mUrl;
    }

    public String getAppStatus() {
        return mAppStatus;
    }

    public String getUpdateStatus() {
        return mUpdateStatus;
    }

    private void resetParsedData() {
        mDaystartSeconds = null;
        mNewVersion = null;
        mUrl = null;
        mUpdateStatus = null;
        mAppStatus = null;

        mParsedInstallEvent = false;
        mParsedPing = false;
        mParsedUpdatecheck = false;
    }

    private boolean logError(Node node, int errorCode) throws RequestFailureException {
        String errorMessage = "Failed to parse: " + node.tag;
        if (mStrictParsingMode) throw new RequestFailureException(errorMessage, errorCode);

        Log.e(TAG, errorMessage);
        return false;
    }

    private void parseRootNode(Node rootNode) throws RequestFailureException {
        for (int i = 0; i < rootNode.children.size(); ++i) {
            if (TextUtils.equals(TAG_RESPONSE, rootNode.children.get(i).tag)) {
                if (parseResponseNode(rootNode.children.get(i))) return;
                break;
            }
        }

        // The tag was bad; reset all of our state and bail.
        resetParsedData();
        logError(rootNode, RequestFailureException.ERROR_PARSE_ROOT);
    }

    private boolean parseResponseNode(Node node) throws RequestFailureException {
        boolean success = true;
        String serverType = node.attributes.get("server");
        success &= TextUtils.equals("3.0", node.attributes.get("protocol"));

        if (!TextUtils.equals("prod", serverType)) Log.w(TAG, "Server type: " + serverType);

        for (int i = 0; i < node.children.size(); ++i) {
            Node current = node.children.get(i);
            if (TextUtils.equals(TAG_DAYSTART, current.tag)) {
                success &= parseDaystartNode(current);
            } else if (TextUtils.equals(TAG_APP, current.tag)) {
                success &= parseAppNode(current);
            } else {
                Log.w(TAG, "Ignoring unknown child of <" + node.tag + "> : " + current.tag);
            }
        }

        if (!success) {
            return logError(node, RequestFailureException.ERROR_PARSE_RESPONSE);
        } else if (mDaystartSeconds == null) {
            return logError(node, RequestFailureException.ERROR_PARSE_DAYSTART);
        } else if (mAppStatus == null) {
            return logError(node, RequestFailureException.ERROR_PARSE_APP);
        } else if (mExpectInstallEvent != mParsedInstallEvent) {
            return logError(node, RequestFailureException.ERROR_PARSE_EVENT);
        } else if (mExpectPing != mParsedPing) {
            return logError(node, RequestFailureException.ERROR_PARSE_PING);
        } else if (mExpectUpdatecheck != mParsedUpdatecheck) {
            return logError(node, RequestFailureException.ERROR_PARSE_UPDATECHECK);
        }

        return true;
    }

    private boolean parseDaystartNode(Node node) throws RequestFailureException {
        try {
            mDaystartSeconds = Integer.parseInt(node.attributes.get("elapsed_seconds"));
            mDaystartDays = Integer.parseInt(node.attributes.get("elapsed_days"));
        } catch (NumberFormatException e) {
            return logError(node, RequestFailureException.ERROR_PARSE_DAYSTART);
        }
        return true;
    }

    private boolean parseAppNode(Node node) throws RequestFailureException {
        boolean success = true;
        success &= TextUtils.equals(mAppId, node.attributes.get("appid"));

        mAppStatus = node.attributes.get("status");
        if (TextUtils.equals("ok", mAppStatus)) {
            for (int i = 0; i < node.children.size(); ++i) {
                Node current = node.children.get(i);
                if (TextUtils.equals(TAG_UPDATECHECK, current.tag)) {
                    success &= parseUpdatecheck(current);
                } else if (TextUtils.equals(TAG_EVENT, current.tag)) {
                    parseEvent(current);
                } else if (TextUtils.equals(TAG_PING, current.tag)) {
                    parsePing(current);
                }
            }
        } else if (TextUtils.equals("restricted", mAppStatus)) {
            // Omaha isn't allowed to get data in this country.  Pretend the request was fine.
        } else {
            success = false;
        }

        if (success) return true;
        return logError(node, RequestFailureException.ERROR_PARSE_APP);
    }

    private boolean parseUpdatecheck(Node node) throws RequestFailureException {
        boolean success = true;

        mUpdateStatus = node.attributes.get("status");
        if (TextUtils.equals("ok", mUpdateStatus)) {
            for (int i = 0; i < node.children.size(); ++i) {
                Node current = node.children.get(i);
                if (TextUtils.equals(TAG_URLS, current.tag)) {
                    parseUrls(current);
                } else if (TextUtils.equals(TAG_MANIFEST, current.tag)) {
                    parseManifest(current);
                }
            }

            // Confirm all the tags we expected to see were parsed properly.
            if (mUrl == null) {
                return logError(node, RequestFailureException.ERROR_PARSE_URLS);
            } else if (mNewVersion == null) {
                return logError(node, RequestFailureException.ERROR_PARSE_MANIFEST);
            }
        } else if (TextUtils.equals("noupdate", mUpdateStatus)) {
            // No update is available.  Don't bother searching for other attributes.
        } else if (mUpdateStatus != null && mUpdateStatus.startsWith("error")) {
            Log.w(TAG, "Ignoring error status for " + node.tag + ": " + mUpdateStatus);
        } else {
            Log.w(TAG, "Ignoring unknown status for " + node.tag + ": " + mUpdateStatus);
        }

        mParsedUpdatecheck = true;
        return true;
    }

    private void parsePing(Node node) {
        if (TextUtils.equals("ok", node.attributes.get("status"))) mParsedPing = true;
    }

    private void parseEvent(Node node) {
        if (TextUtils.equals("ok", node.attributes.get("status"))) mParsedInstallEvent = true;
    }

    private void parseUrls(Node node) {
        for (int i = 0; i < node.children.size(); ++i) {
            Node current = node.children.get(i);
            if (TextUtils.equals(TAG_URL, current.tag)) parseUrl(current);
        }
    }

    private void parseUrl(Node node) {
        String url = node.attributes.get("codebase");
        if (url == null) return;

        // The URL gets a "/" tacked onto it by the server.  Remove it.
        if (url.endsWith("/")) url = url.substring(0, url.length() - 1);
        mUrl = url;
    }

    private void parseManifest(Node node) {
        mNewVersion = node.attributes.get("version");
    }
}
