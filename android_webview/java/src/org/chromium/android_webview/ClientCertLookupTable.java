// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import java.security.PrivateKey;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Store user's client certificate decision for a host and port pair. Not
 * thread-safe. All accesses are done on UI thread.
 */
public class ClientCertLookupTable {

    /** A container for the certificate data. */
    public static class Cert {
        PrivateKey mPrivateKey;
        byte[][] mCertChain;

        public Cert(PrivateKey privateKey, byte[][] certChain) {
            this.mPrivateKey = privateKey;
            byte[][] newChain = new byte[certChain.length][];
            for (int i = 0; i < certChain.length; i++) {
                newChain[i] = Arrays.copyOf(certChain[i], certChain[i].length);
            }
            this.mCertChain = newChain;
        }
    }

    private final Map<String, Cert> mCerts;
    private final Set<String> mDenieds;

    // Clear client certificate preferences
    public void clear() {
        mCerts.clear();
        mDenieds.clear();
    }

    public ClientCertLookupTable() {
        mCerts = new HashMap<String, Cert>();
        mDenieds = new HashSet<String>();
    }

    public void allow(String host, int port, PrivateKey privateKey, byte[][] chain) {
        String host_and_port = hostAndPort(host, port);
        mCerts.put(host_and_port, new Cert(privateKey, chain));
        mDenieds.remove(host_and_port);
    }

    public void deny(String host, int port) {
        String host_and_port = hostAndPort(host, port);
        mCerts.remove(host_and_port);
        mDenieds.add(host_and_port);
    }

    public Cert getCertData(String host, int port) {
        return mCerts.get(hostAndPort(host, port));
    }

    public boolean isDenied(String host, int port) {
        return mDenieds.contains(hostAndPort(host, port));
    }

    // TODO(sgurun) add a test for this. Not separating host and pair properly will be
    // a security issue.
    private static String hostAndPort(String host, int port) {
        return host + ":" + port;
    }
}
