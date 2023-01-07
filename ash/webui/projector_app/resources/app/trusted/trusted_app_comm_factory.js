// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from 'chrome://resources/ash/common/post_message_api/post_message_api_client.js';
import {RequestHandler} from 'chrome://resources/ash/common/post_message_api/post_message_api_request_handler.js';

import {ProjectorBrowserProxy, ProjectorBrowserProxyImpl} from './projector_browser_proxy.js';

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
   * Notfies the app whether it can start a new session or not.
   * @param {!projectorApp.NewScreencastPreconditionState} newState
   * @return {Promise<boolean>}
   */
  onNewScreencastPreconditionChanged(newState) {
    return this.callApiFn('onNewScreencastPreconditionChanged', [newState]);
  }

  /**
   * Notifies the Projector App the download and installation progress of the
   * SODA binary and language packs.
   * @param {number} progress A number in range 0 -100 indicating installation
   *     progress.
   */
  onSodaInstallProgressUpdated(progress) {
    return this.callApiFn('onSodaInstallProgressUpdated', [progress]);
  }

  /**
   * Notifies the Projector App when SODA download and installation is complete.
   */
  onSodaInstalled() {
    return this.callApiFn('onSodaInstalled', []);
  }

  /**
   * Notifies the Projector App when there is a SODA installation error.
   */
  onSodaInstallError() {
    return this.callApiFn('onSodaInstallError', []);
  }

  /**
   * Notfies the app when screencasts' pending state have changed.
   * @param {!Array<!projectorApp.PendingScreencast>} pendingScreencasts
   */
  onScreencastsStateChange(pendingScreencasts) {
    return this.callApiFn('onScreencastsStateChange', pendingScreencasts);
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
 * Class that implements the RequestHandler inside the Projector trusted scheme
 * for the Projector App.
 */
export class TrustedAppRequestHandler extends RequestHandler {
  /*
   * @param {!Element} iframeElement The <iframe> element to listen to as a
   *     client.
   * @param {ProjectorBrowserProxy} browserProxy The browser proxy that will be
   *     used to handle the messages.
   */
  constructor(iframeElement, browserProxy) {
    super(iframeElement, TARGET_URL, TARGET_URL);
    this.browserProxy_ = browserProxy;

    this.registerMethod('getAccounts', (args) => {
      return this.browserProxy_.getAccounts();
    });
    this.registerMethod('getNewScreencastPreconditionState', (args) => {
      return this.browserProxy_.getNewScreencastPreconditionState();
    });
    this.registerMethod('startProjectorSession', (storageDir) => {
      if (!storageDir || storageDir.length != 1) {
        return false;
      }
      return this.browserProxy_.startProjectorSession(storageDir[0]);
    });
    this.registerMethod('getOAuthTokenForAccount', (args) => {
      if (!args || args.length != 1) {
        return Promise.reject('Incorrect args for getOAuthTokenForAccount');
      }
      return this.browserProxy_.getOAuthTokenForAccount(args[0]);
    });
    this.registerMethod('onError', (msg) => {
      this.browserProxy_.onError(msg);
    });
    this.registerMethod('sendXhr', (values) => {
      if (!values || values.length != 7) {
        return {
          success: false,
          error: 'INVALID_ARGUMENTS',
        };
      }
      return this.browserProxy_.sendXhr(
          values[0], values[1], values[2], values[3], values[4], values[5],
          values[6]);
    });
    this.registerMethod('shouldDownloadSoda', (args) => {
      return this.browserProxy_.shouldDownloadSoda();
    });
    this.registerMethod('installSoda', (args) => {
      return this.browserProxy_.installSoda();
    });
    this.registerMethod('getPendingScreencasts', (args) => {
      return this.browserProxy_.getPendingScreencasts();
    });
    this.registerMethod('getUserPref', (args) => {
      if (!args || args.length != 1) {
        return;
      }
      return this.browserProxy_.getUserPref(args[0]);
    });
    this.registerMethod('setUserPref', (args) => {
      if (!args || args.length != 2) {
        return;
      }
      return this.browserProxy_.setUserPref(args[0], args[1]);
    });
    this.registerMethod('openFeedbackDialog', (args) => {
      return this.browserProxy_.openFeedbackDialog();
    });
    this.registerMethod('getVideo', (args) => {
      if (!args || args.length != 2) {
        return Promise.reject('Incorrect args for getVideo');
      }
      return this.browserProxy_.getVideo(args[0], args[1]);
    });
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

    AppTrustedCommFactory.requestHandler_ = new TrustedAppRequestHandler(
        iframeElement, ProjectorBrowserProxyImpl.getInstance());
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
