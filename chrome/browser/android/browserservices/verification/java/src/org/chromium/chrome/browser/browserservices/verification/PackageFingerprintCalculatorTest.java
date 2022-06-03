// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import static org.chromium.chrome.browser.browserservices.verification.PackageFingerprintCalculator.byteArrayToHexString;
import static org.chromium.chrome.browser.browserservices.verification.PackageFingerprintCalculator.getCertificateSHA256FingerprintForPackage;

import android.content.pm.PackageManager;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for {@link PackageFingerprintCalculator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(OriginVerifierTest.TEST_BATCH_NAME)
public class PackageFingerprintCalculatorTest {
    private static final byte[] BYTE_ARRAY = new byte[] {(byte) 0xaa, (byte) 0xbb, (byte) 0xcc,
            (byte) 0x10, (byte) 0x20, (byte) 0x30, (byte) 0x01, (byte) 0x02};
    private static final String STRING_ARRAY = "AA:BB:CC:10:20:30:01:02";

    private static final String SHA_256_FINGERPRINT_PUBLIC =
            "32:A2:FC:74:D7:31:10:58:59:E5:A8:5D:F1:6D:95:F1:02:D8:5B"
            + ":22:09:9B:80:64:C5:D8:91:5C:61:DA:D1:E0";
    private static final String SHA_256_FINGERPRINT_OFFICIAL =
            "19:75:B2:F1:71:77:BC:89:A5:DF:F3:1F:9E:64:A6:CA:E2:81:A5"
            + ":3D:C1:D1:D5:9B:1D:14:7F:E1:C8:2A:FA:00";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    @Test
    @SmallTest
    public void testSHA256CertificateChecks() {
        Assert.assertEquals(STRING_ARRAY, byteArrayToHexString(BYTE_ARRAY));

        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        String fingerprint = getCertificateSHA256FingerprintForPackage(pm, PACKAGE_NAME);

        // We could try to determine which fingerprint we should be signed with, but it's easier to
        // just check that we match either of the fingerprints. The chances of our code returning
        // an incorrect value that just happens to match the wrong fingerprint is incredibly small.
        if (SHA_256_FINGERPRINT_OFFICIAL.equals(fingerprint)) return;
        if (SHA_256_FINGERPRINT_PUBLIC.equals(fingerprint)) return;

        Assert.fail("Generated fingerprint matches neither official nor public.");
    }
}
