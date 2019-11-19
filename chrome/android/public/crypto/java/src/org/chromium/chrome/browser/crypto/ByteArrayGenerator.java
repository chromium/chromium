// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crypto;

import java.io.FileInputStream;
import java.io.IOException;
import java.security.GeneralSecurityException;

/**
 * Reads /dev/urandom to generate byte arrays for used in cryptographic functions.
 */
public class ByteArrayGenerator {
    /**
     * Polls random data to generate the array.
     * @param numBytes Length of the array to generate.
     * @return byte[] containing randomly generated data.
     */
    public byte[] getBytes(int numBytes) throws IOException, GeneralSecurityException {
        FileInputStream fis = null;
        try {
            fis = new FileInputStream("/dev/urandom");
            byte[] bytes = new byte[numBytes];
            if (bytes.length != fis.read(bytes)) {
                throw new GeneralSecurityException("Not enough random data available");
            }
            return bytes;
        } finally {
            if (fis != null) {
                fis.close();
            }
        }
    }
}
