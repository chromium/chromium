// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.app.Instrumentation;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.TestAwContentsClient;

/** Code shared between the various video tests. */
public class VideoTestUtil {
    /**
     * Run video test.
     * @param instr the test instrumentation
     * @param testRule the test rule instance we're going to run the test in.
     * @param requiredUserGesture the settings of MediaPlaybackRequiresUserGesture.
     * @return true if the event happened,
     * @throws Throwable throw exception if timeout.
     */
    public static boolean runVideoTest(
            final Instrumentation instr,
            final AwActivityTestRule testRule,
            final boolean requiredUserGesture,
            long waitTime)
            throws Throwable {
        final JavascriptEventObserver observer = new JavascriptEventObserver();
        TestAwContentsClient client = new TestAwContentsClient();
        final AwContents awContents =
                testRule.createAwTestContainerViewOnMainSync(client).getAwContents();
        instr.runOnMainSync(
                () -> {
                    AwSettings awSettings = awContents.getSettings();
                    awSettings.setJavaScriptEnabled(true);
                    awSettings.setMediaPlaybackRequiresUserGesture(requiredUserGesture);
                    observer.register(awContents.getWebContents(), "javaObserver");
                });
        VideoTestWebServer webServer = new VideoTestWebServer();
        try {
            String data =
                    "<html><head><script>"
                            + "addEventListener('DOMContentLoaded', function() { "
                            + "  document.getElementById('video').addEventListener('play', function() { "
                            + "    javaObserver.notifyJava(); "
                            + "  }, false); "
                            + "}, false); "
                            + "</script></head><body>"
                            + "<video id='video' autoplay control src='"
                            + webServer.getOnePixelOneFrameWebmURL()
                            + "' /> </body></html>";
            testRule.loadDataAsync(awContents, data, "text/html", false);
            return observer.waitForEvent(waitTime);
        } finally {
            if (webServer != null && webServer.getTestWebServer() != null) {
                webServer.getTestWebServer().shutdown();
            }
        }
    }
}
