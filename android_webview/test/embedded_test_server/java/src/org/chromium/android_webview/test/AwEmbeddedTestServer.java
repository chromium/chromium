// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.content.Intent;

import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/**
 * A simple file server for java android webview tests which extends
 *  from class EmbeddedTestServer. It is able to add custom handlers
 *  which registers with net::test_server::EmbeddedTestServer native code.
 *
 * An example use:
 *   AwEmbeddedTestServer s = AwEmbeddedTestServer.createAndStartServer(context);
 *
 *   // serve requests...
 *   s.getURL("/foo/bar.txt");
 *
 *   // Generally safe to omit as ResettersForTesting will call it.
 *   s.stopAndDestroyServer();
 * Note that this runs net::test_server::EmbeddedTestServer in a service in a separate APK.
 */
public class AwEmbeddedTestServer extends EmbeddedTestServer {
    /** Set intent package and class name that will pass to the service.
     *
     *  @param intent The intent to use to pass into the service.
     */
    @Override
    protected void setIntentClassName(Intent intent) {
        intent.setClassName(
                "org.chromium.android_webview.test.support",
                "org.chromium.android_webview.test.AwEmbeddedTestServerService");
    }

    /** Create and initialize a server with the default and custom handlers.
     *
     *  This handles native object initialization, server configuration, and server initialization.
     *  On returning, the server is ready for use.
     *
     *  @param context The context in which the server will run.
     *  @return The created server.
     */
    public static AwEmbeddedTestServer createAndStartServer(Context context) {
        return initializeAndStartServer(new AwEmbeddedTestServer(), context, /* port= */ 0);
    }

    /**
     * Create and initialize an HTTPS server with the default and custom handlers.
     *
     *  This handles native object initialization, server configuration, and server initialization.
     *  On returning, the server is ready for use.
     *
     *  @param context The context in which the server will run.
     *  @param serverCertificate The certificate option that the server will use.
     *  @return The created server.
     */
    public static AwEmbeddedTestServer createAndStartHTTPSServer(
            Context context, @ServerCertificate int serverCertificate) {
        return initializeAndStartHTTPSServer(
                new AwEmbeddedTestServer(), context, serverCertificate, /* port= */ 0);
    }
}
