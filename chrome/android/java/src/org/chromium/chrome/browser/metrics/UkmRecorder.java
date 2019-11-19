// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * An interface and classes to record User Keyed Metrics.
 */
public abstract class UkmRecorder {
    /**
     * Records the occurrence of a (boolean) UKM event with name |eventName|.
     * A UKM entry with |eventName| must be present in ukm.xml with a metric named `HasOccured`.
     * For example,
     * <event name="SomeFeature.SomeComponent">
     * <owner>owner@chromium.org</owner>
     * <summary>
     * User triggered a specific feature.
     * </summary>
     * <metric name="HasOccurred" enum="Boolean">
     * <summary>
     * A boolean signaling that the event has occurred (typically only records
     *  true values).
     * </summary>
     * </metric>
     * </event>
     */
    abstract void recordEvent(WebContents webContents, String eventName);

    /**
     * The actual recorder.
     */
    @JNINamespace("metrics")
    public static class Bridge extends UkmRecorder {
        @Override
        public void recordEvent(WebContents webContents, String eventName) {
            UkmRecorderJni.get().recordEvent(webContents, eventName);
        }
    }

    @NativeMethods
    interface Natives {
        void recordEvent(WebContents webContents, String eventName);
    }
}
