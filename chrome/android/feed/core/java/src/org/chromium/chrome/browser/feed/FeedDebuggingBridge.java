// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.client.scope.ProcessScope;
import com.google.android.libraries.feed.common.logging.Dumper;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.io.StringWriter;

/**
 * Java-side bridge that receives and responds to native-initiated requests for debugging data. This
 * data is then used to populate the Feed Internals page.
 */
@JNINamespace("feed")
public class FeedDebuggingBridge {
    @CalledByNative
    static String getFeedFetchUrl() {
        return FeedConfiguration.getFeedServerEndpoint();
    }

    @CalledByNative
    static String getProcessScopeDump() {
        ProcessScope processScope = FeedProcessScopeFactory.getFeedProcessScope();
        Dumper dumper = Dumper.newDefaultDumper();
        dumper.dump(processScope);

        try {
            StringWriter writer = new StringWriter();
            dumper.write(writer);
            return writer.toString();
        } catch (IOException e) {
            return "Unable to dump ProcessScope";
        }
    }

    @CalledByNative
    static void triggerRefresh() {
        ProcessScope processScope = FeedProcessScopeFactory.getFeedProcessScope();

        // Do nothing if Feed is disabled.
        if (processScope == null) return;

        RequestManager requestManager = processScope.getRequestManager();

        // The only notification of this completing is through the SchedulerApi interface, like all
        // other refreshes.
        requestManager.triggerScheduledRefresh();
    }
}
