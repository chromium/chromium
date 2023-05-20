// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';

const TARGET_URL = 'chrome-untrusted://projector/';


// A PostMessageAPIClient that sends messages to chrome-untrusted://projector.
export class UntrustedAppClient extends PostMessageAPIClient {
  /**
   * @param {!Window} targetWindow
   */
  constructor(targetWindow) {
    super(TARGET_URL, targetWindow);
  }

  /**
   * Notifies the untrusted context when a new video file is available.
   * @param {string} videoFileId the Drive item id of the video file.
   * @param {?File} videoFile to provide to the untrusted context.
   * @param {?DOMException} error if retrieving the video file failed.
   */
  onFileLoaded(videoFileId, videoFile, error) {
    return this.callApiFn('onFileLoaded', [videoFileId, videoFile, error]);
  }
}

/**
 * This is a class that is used to setup the duplex communication
 * channels between this origin, chrome://projector/* and the iframe embedded
 * inside the document.
 */
export class AppTrustedCommFactory {
  /**
   * Creates the instances of PostMessageAPIClient and RequestHandler.
   */
  static maybeCreateInstances() {
    if (AppTrustedCommFactory.client_ ||
        AppTrustedCommFactory.requestHandler_) {
      return;
    }

    const iframeElement = document.getElementsByTagName('iframe')[0];

    AppTrustedCommFactory.client_ =
        new UntrustedAppClient(iframeElement.contentWindow);
  }

  /**
   * In order to use this class, please do the following (e.g. to notify the app
   * that it can start a new session):
   * const success = await AppTrustedCommFactory.
   *                 getPostMessageAPIClient().onCanStartNewSession(true);
   * @return {!UntrustedAppClient}
   */
  static getPostMessageAPIClient() {
    // AnnotatorUntrustedCommFactory.client_ should be available. However to be
    // on the cautious side create an instance here if getPostMessageAPIClient
    // is triggered before the page finishes loading.
    AppTrustedCommFactory.maybeCreateInstances();
    return AppTrustedCommFactory.client_;
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Create instances of the singletons (PostMessageAPIClient and
  // RequestHandler) when the document has finished loading.
  AppTrustedCommFactory.maybeCreateInstances();
});
