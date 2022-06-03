// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * junit tests for {@link PiiElider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PiiEliderTest {
    private static final int MAX_LINES = 5;

    @Test
    public void testElideEmail() {
        String original = "email me at someguy@mailservice.com";
        String expected = "email me at XXX@EMAIL.ELIDED";
        assertEquals(expected, PiiElider.elideEmail(original));
    }

    @Test
    public void testElideUrl() {
        String original = "file bugs at crbug.com";
        String expected = "file bugs at HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideUrl2() {
        String original = "exception at org.chromium.base.PiiEliderTest";
        assertEquals(original, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideUrl3() {
        String original = "file bugs at crbug.com or code.google.com";
        String expected = "file bugs at HTTP://WEBADDRESS.ELIDED or HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideUrl4() {
        String original = "test shorturl.com !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideUrl5() {
        String original = "test just.the.perfect.len.url !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideUrl6() {
        String original = "test a.very.very.very.very.very.long.url !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideUrl7() {
        String original = " at android.content.Intent \n at java.util.ArrayList";
        assertEquals(original, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideUrl8() {
        String original = "exception at org.chromium.chrome.browser.compositor.scene_layer."
                + "TabListSceneLayer.nativeUpdateLayer(Native Method)";
        assertEquals(original, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideUrl9() {
        String original = "I/dalvikvm( 5083): at org.chromium.chrome.browser.compositor."
                + "scene_layer.TabListSceneLayer.nativeUpdateLayer(Native Method)";
        assertEquals(original, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideNonHttpUrl() {
        String original = "test some-other-scheme://address/01010?param=33&other_param=AAA !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, PiiElider.elideUrl(original));
    }

    @Test
    public void testDontElideFileSuffixes() {
        String original = "chromium_android_linker.so";
        assertEquals(original, PiiElider.elideUrl(original));
    }

    @Test
    public void testElideIp() {
        String original = "traceroute 127.0.0.1";
        String expected = "traceroute 1.2.3.4";
        assertEquals(expected, PiiElider.elideIp(original));
    }

    @Test
    public void testElideMac1() {
        String original = "MAC: AB-AB-AB-AB-AB-AB";
        String expected = "MAC: 01:23:45:67:89:AB";
        assertEquals(expected, PiiElider.elideMac(original));
    }

    @Test
    public void testElideMac2() {
        String original = "MAC: AB:AB:AB:AB:AB:AB";
        String expected = "MAC: 01:23:45:67:89:AB";
        assertEquals(expected, PiiElider.elideMac(original));
    }

    @Test
    public void testElideConsole() {
        String original = "I/chromium(123): [INFO:CONSOLE(2)] hello!";
        String expected = "I/chromium(123): [ELIDED:CONSOLE(0)] ELIDED CONSOLE MESSAGE";
        assertEquals(expected, PiiElider.elideConsole(original));
    }

    @Test
    public void testElideUrlInStacktrace() {
        String original = "java.lang.RuntimeException: Outer Exception crbug.com/12345\n"
                + "  at org.chromium.base.PiiElider.sanitizeStacktrace (PiiElider.java:120)\n"
                + "Caused by: java.lang.NullPointerException: Inner Exception shorturl.com/bxyj5";
        String expected = "java.lang.RuntimeException: Outer Exception HTTP://WEBADDRESS.ELIDED\n"
                + "  at org.chromium.base.PiiElider.sanitizeStacktrace (PiiElider.java:120)\n"
                + "Caused by: java.lang.NullPointerException: Inner Exception "
                + "HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, PiiElider.sanitizeStacktrace(original));
    }

    @Test
    public void testDoesNotElideMethodNameInStacktrace() {
        String original = "java.lang.NullPointerException: Attempt to invoke virtual method 'int "
                + "androidx.fragment.app.FragmentManager.getBackStackEntryCount()' on a null "
                + "object reference";
        assertEquals(original, PiiElider.sanitizeStacktrace(original));
    }
}
