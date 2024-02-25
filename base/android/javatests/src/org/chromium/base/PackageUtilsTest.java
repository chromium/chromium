// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.hamcrest.CoreMatchers.anyOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import static org.chromium.base.PackageUtils.byteArrayToHexString;
import static org.chromium.base.PackageUtils.getCertificateSHA256FingerprintForPackage;

import android.content.pm.PackageManager;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.util.Collections;
import java.util.List;

/** Tests for {@link PackageUtils}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PackageUtilsTest {
    private static final byte[] BYTE_ARRAY =
            new byte[] {
                (byte) 0xaa,
                (byte) 0xbb,
                (byte) 0xcc,
                (byte) 0x10,
                (byte) 0x20,
                (byte) 0x30,
                (byte) 0x01,
                (byte) 0x02
            };
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
    public void testByteArrayToHexString() {
        Assert.assertEquals(STRING_ARRAY, byteArrayToHexString(BYTE_ARRAY));
    }

    @Test
    @SmallTest
    public void testSHA256CertificateChecks() {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        List<String> fingerprints = getCertificateSHA256FingerprintForPackage(PACKAGE_NAME);

        assertThat(
                fingerprints,
                anyOf(
                        is(Collections.singletonList(SHA_256_FINGERPRINT_PUBLIC)),
                        is(Collections.singletonList(SHA_256_FINGERPRINT_OFFICIAL))));
    }
}
