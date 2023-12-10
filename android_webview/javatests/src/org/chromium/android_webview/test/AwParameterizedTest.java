// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.webkit.WebSettings;

import org.junit.runners.Parameterized.Parameters;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.base.CommandLine;

/**
 * An abstract base class for parameterized Android Webview instrumentation tests.
 * The shared parameter provider method returns a list of mutations to default
 * AwSettings under which the parameterized tests should run.
 */
public abstract class AwParameterizedTest {
    @Parameters(name = "{0}")
    public static Object[] data() {
        if (!CommandLine.getInstance().hasSwitch("webview-mutations-enabled")) {
            return new Object[] {AwSettingsMutation.doNotMutateAwSettings()};
        }
        return new Object[] {
            AwSettingsMutation.doNotMutateAwSettings(),
            new AwSettingsMutation(
                    settings -> {
                        settings.setAllowFileAccess(true);
                        settings.setAllowFileAccessFromFileURLs(true);
                        settings.setAllowUniversalAccessFromFileURLs(true);
                        settings.setBuiltInZoomControls(true);
                        settings.setDatabaseEnabled(true);
                        settings.setDisplayZoomControls(false);
                        settings.setDomStorageEnabled(true);
                        settings.setImagesEnabled(false);
                        settings.setJavaScriptCanOpenWindowsAutomatically(true);
                        settings.setJavaScriptEnabled(true);
                        settings.setLoadWithOverviewMode(true);
                        settings.setMediaPlaybackRequiresUserGesture(false);
                        settings.setShouldFocusFirstNode(false);
                        settings.setSupportMultipleWindows(true);
                        settings.setUseWideViewPort(true);
                        AwContentsStatics.setRecordFullDocument(true);
                        settings.setSupportZoom(false);
                        settings.setAllowContentAccess(false);
                        settings.setGeolocationEnabled(false);
                        settings.setCacheMode(WebSettings.LOAD_NO_CACHE);
                        settings.setDisabledActionModeMenuItems(
                                WebSettings.MENU_ITEM_SHARE | WebSettings.MENU_ITEM_WEB_SEARCH);
                        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
                        settings.setDefaultFontSize(42);
                        settings.setTextZoom(200);
                        settings.setUserAgentString("foobar");
                    },
                    "allMutations..true")
        };
    }
}
