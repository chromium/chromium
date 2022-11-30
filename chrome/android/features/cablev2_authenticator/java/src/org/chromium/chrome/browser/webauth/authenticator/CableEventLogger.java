// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

/**
 * Abstracts over an injectable log sink.
 *
 * <p>In order to support log destinations that are only reachable from from internal-only code,
 * this class holds a static reference to an interface that abstracts over the log. This logging is
 * only for cases where Google already has knowledge of the transaction, i.e. sign-ins to
 * accounts.google.com.
 */
public class CableEventLogger {
    /**
     * Abstracts a protobuf log destination
     */
    public interface Sink {
        void log(byte[] event);
    }

    private static Sink sExternalSink;

    /**
     * Configure a global log destination.
     *
     * @param sink the log that will receive any calls to {@link log}.
     */
    public static void setExternalSink(Sink sink) {
        sExternalSink = sink;
    }

    /**
     * Log a protobuf if a global log has been configured.
     */
    public static void log(byte[] event) {
        if (sExternalSink != null) {
            sExternalSink.log(event);
        }
    }
}
