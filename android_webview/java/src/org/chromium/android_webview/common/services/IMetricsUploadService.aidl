// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

/**
 * The service is exported and this interface is available to WebViews embedded in all apps to use.
 */
interface IMetricsUploadService {
   /**
    * Send the given UMA log to the clearcut service in GMS core on the device.
    *
    * @param serializedLog the serialized bytes of the ChromeUserMetricsExtension proto message.
    *
    * @returns an integer HTTP status code indicating the success state of sending the log to the
    *       platform.
    */
   int uploadMetricsLog(in byte[] serializedLog);
}
