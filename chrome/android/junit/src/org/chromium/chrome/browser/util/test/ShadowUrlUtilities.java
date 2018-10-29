// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util.test;

import android.text.TextUtils;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.chrome.browser.util.UrlUtilities;

/** Implementation of UrlUtilities which does not rely on native. */
@Implements(UrlUtilities.class)
public class ShadowUrlUtilities {

    private static TestImpl sTestImpl = new TestImpl();

    /** Set implementation for tests. Don't forget to call {@link #reset} later. */
    public static void setTestImpl(TestImpl impl) {
        sTestImpl = impl;
    }

    @Resetter
    public static void reset() {
        sTestImpl = new TestImpl();
    }

    @Implementation
    public static boolean urlsMatchIgnoringFragments(String url, String url2) {
        return sTestImpl.urlsMatchIgnoringFragments(url, url2);
    }

    @Implementation
    public static String getDomainAndRegistry(String uri, boolean includePrivateRegistries) {
        return sTestImpl.getDomainAndRegistry(uri, includePrivateRegistries);
    }

    /** Default implementation for tests. Override methods or add new ones as necessary. */
    public static class TestImpl {
        public boolean urlsMatchIgnoringFragments(String url, String url2) {
            return TextUtils.equals(url, url2);
        }

        public String getDomainAndRegistry(String uri, boolean includePrivateRegistries) {
            return uri;
        }
    }
}
