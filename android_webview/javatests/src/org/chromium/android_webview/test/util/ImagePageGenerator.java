// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.chromium.net.test.util.TestWebServer;

/**
 * The purpose of the generator is to provide a sequence of distinct images to
 * avoid caching side-effects. As we don't need too many images, I've found it
 * easier to hardcode image samples. It is possible to generate images on the
 * fly, but it will require hooking up additional packages.
 */
public class ImagePageGenerator {

    public static final String IMAGE_LOADED_STRING = "1";
    public static final String IMAGE_NOT_LOADED_STRING = "0";

    private static final String[] COLORS = {
        "AAAAIAAc3j0Ss", "AQABIAEayS9b0", "AgACIAIQ8BmAc", "AwADIAMW5wvJE",
        "BAAEIAQZNWRTI", "BQAFIAUfInYaQ", "BgAGIAYVG0DB4", "BwAHIAcTDFKIg",
        "CAAIIAgXCI+Rk", "CQAJIAkRH53Y8", "CgAKIAobJqsDU", "CwALIAsdMblKM",
        "DAAMIAwS49bQA", "DQANIA0U9MSZY", "DgAOIA4ezfJCw", "DwAPIA8Y2uALo",
        "D+AQAA/9vaUwc", "D/AQEBANNhzkw"
    };

    private static final String IMAGE_PREFIX =
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA"
                    + "6fptVAAAAAXNSR0IArs4c6QAAAA1JREFUCB0BAgD9/w";

    private static final String IMAGE_SUFFIX = "AAAAASUVORK5CYII=";

    private int mIndex;
    private final boolean mAdvance;

    public ImagePageGenerator(int startIndex, boolean advance) {
        mIndex = startIndex;
        mAdvance = advance;
    }

    public String getImageSourceNoAdvance() {
        return IMAGE_PREFIX + COLORS[mIndex] + IMAGE_SUFFIX;
    }

    public String getPageTemplateSource(String imageSrc) {
        return CommonResources.getOnImageLoadedHtml(imageSrc);
    }

    public String getPageSource() {
        String result = getPageTemplateSource("data:image/png;base64," + getImageSourceNoAdvance());
        if (mAdvance) mIndex += 2;
        return result;
    }

    public String getPageUrl(TestWebServer webServer) {
        final String imagePath = "/image_" + mIndex + ".png";
        final String pagePath = "/html_image_" + mIndex + ".html";
        webServer.setResponseBase64(
                imagePath, getImageSourceNoAdvance(), CommonResources.getImagePngHeaders(false));
        if (mAdvance) mIndex += 2;
        return webServer.setResponse(pagePath, getPageTemplateSource(imagePath), null);
    }
}
