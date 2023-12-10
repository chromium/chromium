// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.graphics.Color;
import android.util.Pair;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/** Auxiliary class providing common HTML and base64 resources using for testing. */
public class CommonResources {

    // Content-type headers used for HTML code.
    public static List<Pair<String, String>> getTextHtmlHeaders(boolean disableCache) {
        return getContentTypeAndCacheHeaders("text/html", disableCache);
    }

    // Content-type headers used for javascript code.
    public static List<Pair<String, String>> getTextJavascriptHeaders(boolean disableCache) {
        return getContentTypeAndCacheHeaders("text/javascript", disableCache);
    }

    // Content-type headers used for png images.
    public static List<Pair<String, String>> getImagePngHeaders(boolean disableCache) {
        return getContentTypeAndCacheHeaders("image/png", disableCache);
    }

    public static List<Pair<String, String>> getContentTypeAndCacheHeaders(
            String contentType, boolean disableCache) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", contentType));
        if (disableCache) headers.add(Pair.create("Cache-Control", "no-store"));
        return headers;
    }

    // Returns the HTML code used to verify if an image has been successfully loaded.
    public static String getOnImageLoadedHtml(String imageSrc) {
        return "<html>"
                + "  <head>"
                + "    <script>"
                + "      function updateTitle() {"
                + "        document.title=document.getElementById('img').naturalHeight"
                + "      }"
                + "    </script>"
                + "  </head>"
                + "  <body onload='updateTitle();'>"
                + "    <img id='img' onload='updateTitle();' src='"
                + imageSrc
                + "'>"
                + "  </body>"
                + "</html>";
    }

    // Default name for the favicon image.
    public static final String FAVICON_FILENAME = "favicon.png";

    // Default name for the test image.
    public static final String TEST_IMAGE_FILENAME = "testimage.png";

    public static final String ASSET_LINKS_PATH = "/.well-known/assetlinks.json";

    // HTML code of a static simple page with a favicon.
    public static final String FAVICON_STATIC_HTML =
            "<html><head><link rel=\"icon\" type=\"image/png\" href=\""
                    + FAVICON_FILENAME
                    + "\">"
                    + "</head><body>Favicon example</body></html>";

    // Base64 data of a 256x256 png with all pixels having colour value 0x0000ff
    public static final String BLUE_PNG_BASE64 =
            "iVBORw0KGgoAAAANSUhEUgAAAQAAAAEACAIAAADTED8xAAACvUlEQVR4nO3TMQEA"
                    + "IAzAsIF/zyBjRxMFfXpm3kDV3Q6ATQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCk"
                    + "GYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5Bm"
                    + "ANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoB"
                    + "SDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYg"
                    + "zQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0"
                    + "A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIM"
                    + "QJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMA"
                    + "aQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCk"
                    + "GYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5Bm"
                    + "ANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoB"
                    + "SDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYg"
                    + "zQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0"
                    + "A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIM"
                    + "QJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMAaQYgzQCkGYA0A5BmANIMQJoBSDMA"
                    + "aQYgzQCkGYA0A5BmANIMQNoHVyEC/zTGc0UAAAAASUVORK5CYII=";

    // Base64 data of a favicon image resource.
    public static final String FAVICON_DATA_BASE64 =
            "iVBORw0KGgoAAAANSUhEUgAAABAAAAAFCAYAAABM6GxJAAAABHNCSVQICAgIfAhkiAAAASJJREFU"
                    + "GJU9yDtLQnEYwOHfOZ40L3gZDJKgJCKaamvpGzS09wUaormh7xA0S5C0ZDTkZJsNUltkkpAUZkIX"
                    + "L3g9FzzH/9vm9vAgoqRUGUu20JHTXFfafUdERJSIKJnOPFUTERHpqIYclY5nb2QKFumky95OlO+W"
                    + "TSgATqOO5k3xr6ZxelXmDFDhdaqfLkPRWQglULaN/V5DPzl3iIb9xCI+Eskog/wdyhowLlb4vThE"
                    + "giF8zRsurx55beg8lMfMezZW9hqz20M/Owhwe2/yUrPI5Ds8//mRehN7JYWxvIX6eWJkbLK9laL8"
                    + "ZrKxFETzxTBNB5SOJjKV/mhCq+uSjGvE4hHc4QA9YGAEwnhWF1ePkCtOWFv0+PiasL8bR3QDr93h"
                    + "HyFup9LWUksHAAAAAElFTkSuQmCC";

    // Default name for an example 'about' HTML page.
    public static final String ABOUT_FILENAME = "about.html";

    // Title used in the 'about' example.
    public static final String ABOUT_TITLE = "About the Google";

    // HTML code of an 'about' example.
    public static final String ABOUT_HTML =
            "<html>"
                    + "  <head>"
                    + "    <title>"
                    + ABOUT_TITLE
                    + "</title>"
                    + "  </head>"
                    + "  <body>"
                    + "    This is the Google!"
                    + "  </body>"
                    + "</html>";

    public static String makeHtmlPageFrom(String headers, String body) {
        return "<html>"
                + "  <head>"
                + "    <style type=\"text/css\">"
                // Make the image take up all of the page so that we don't have to do
                // any fancy hit target calculations when synthesizing the touch event
                // to click it.
                + "      img.big { width:100%; height:100%; background-color:blue; }"
                + "      .full_view { height:100%; width:100%; position:absolute; }"
                + "    </style>"
                + headers
                + "  </head>"
                + "  <body>"
                + body
                + "  </body>"
                + "</html>";
    }

    // The color must match the background color of 'img.big' CSS class.
    public static final int LINK_COLOR = Color.BLUE;

    public static String makeHtmlPageWithSimpleLinkTo(String headers, String destination) {
        return makeHtmlPageFrom(
                headers,
                "<a href=\""
                        + destination
                        + "\" id=\"link\">"
                        + "  <img class=\"big\" />"
                        + "</a>"
                        + "<div>Some text</div>");
    }

    public static String makeHtmlPageWithSimpleLinkTo(String destination) {
        return makeHtmlPageWithSimpleLinkTo("", destination);
    }

    public static String makeHtmlPageWithSimplePostFormTo(String destination) {
        return makeHtmlPageFrom(
                "",
                "<form action=\""
                        + destination
                        + "\" method=\"post\">"
                        + "  <input type=\"submit\" value=\"post\" id=\"link\">"
                        + "</form>");
    }

    public static String makeAssetFile(String fingerprint) {
        try {
            var relationArr = new JSONArray().put("delegate_permission/common.handle_all_urls");
            var targetObj =
                    new JSONObject()
                            .put("namespace", "android_app")
                            .put("package_name", "org.chromium.android_webview.shell")
                            .put("sha256_cert_fingerprints", new JSONArray().put(fingerprint));
            return new JSONArray()
                    .put(new JSONObject().put("relation", relationArr).put("target", targetObj))
                    .toString();
        } catch (JSONException e) {
        }
        return "";
    }
}
