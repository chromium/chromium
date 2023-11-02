// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from '//resources/ash/common/post_message_api/post_message_api_client.js';
import {RequestHandler} from '//resources/ash/common/post_message_api/post_message_api_request_handler.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {ProjectorError} from 'chrome-untrusted://projector/common/message_types.js';

const TARGET_URL = 'chrome://projector/';

// Maps video file id to promises of video files.
const loadingFiles = new Map /*<string, PromiseResolver>*/ ();

function getOrCreateLoadFilePromise(fileId) {
  if (loadingFiles.has(fileId)) {
    return loadingFiles.get(fileId);
  }
  const promise = new PromiseResolver();
  loadingFiles.set(fileId, promise);
  return promise;
}

/**
 * Returns the projector app element inside this current DOM.
 * @return {projectorApp.AppApi}
 */
function getAppElement() {
  return /** @type {projectorApp.AppApi} */ (
      document.querySelector('projector-app'));
}

/**
 * Waits for the projector-app element to exist in the DOM.
 */
function waitForAppElement() {
  return new Promise(resolve => {
    if (getAppElement()) {
      return resolve();
    }

    const observer = new MutationObserver(mutations => {
      if (getAppElement()) {
        resolve();
        observer.disconnect();
      }
    });

    observer.observe(document.body, {childList: true, subtree: true});
  });
}

/**
 * Implements and supports the methods defined by the
 * projectorApp.ClientDelegate interface defined in
 * //ash/webui/projector_app/resources/communication/projector_app.externs.js.
 */
const CLIENT_DELEGATE = {
  /**
   * Gets the list of primary and secondary accounts currently available on the
   * device.
   * @return {Promise<Array<!projectorApp.Account>>}
   */
  getAccounts() {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'getAccounts', []);
  },

  /**
   * Checks whether the SWA can trigger a new Projector session.
   * @return {!Promise<!projectorApp.NewScreencastPreconditionState>}
   */
  getNewScreencastPreconditionState() {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'getNewScreencastPreconditionState', []);
  },

  /**
   * Starts the Projector session if it is possible. Provides the storage dir
   * where  projector output artifacts will be saved in.
   * @param {string} storageDir
   * @return {Promise<boolean>}
   */
  startProjectorSession(storageDir) {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'startProjectorSession', [storageDir]);
  },

  /**
   * Gets the oauth token with the required scopes for the specified account.
   * @param {string} account user's email
   * @return {!Promise<!projectorApp.OAuthToken>}
   */
  getOAuthTokenForAccount(account) {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'getOAuthTokenForAccount', [account]);
  },

  /**
   * Sends 'error' message to handler.
   * The Handler will log the message. If the error is not a recoverable error,
   * the handler closes the corresponding WebUI.
   * @param {!Array<ProjectorError>} msg Error messages.
   */
  onError(msg) {
    AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'onError', [msg]);
  },

  /**
   * Gets the list of pending screencasts that are uploading to drive on current
   * device.
   * @return {Promise<Array<projectorApp.PendingScreencast>>}
   */
  getPendingScreencasts() {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'getPendingScreencasts', []);
  },

  /*
   * Send XHR request.
   * @param {string} url the request URL.
   * @param {string} method the request method.
   * @param {string=} requestBody the request body data.
   * @param {boolean=} useCredentials authorize the request with end user
   *     credentials. Used for getting streaming URL.
   * @param {boolean=} useApiKey authorize the request with API key. Used for
   *     translaton requests.
   * @param {object=} additional headers.
   * @param {string=} account email.
   * @return {!Promise<!projectorApp.XhrResponse>}
   */
  sendXhr(
      url, method, requestBody, useCredentials, useApiKey, headers,
      accountEmail) {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'sendXhr', [
          url,
          method,
          requestBody ? requestBody : '',
          !!useCredentials,
          !!useApiKey,
          headers,
          accountEmail,
        ]);
  },

  /**
   * Returns true if the device supports on device speech recognition.
   * @return {!Promise<boolean>}
   */
  shouldDownloadSoda() {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'shouldDownloadSoda', []);
  },

  /**
   * Triggers the installation of on device speech recognition binary and
   * language packs for the user's locale. Returns true if download and
   * installation started.
   * @return {!Promise<boolean>}
   */
  installSoda() {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'installSoda', []);
  },

  /**
   * Checks if the user has given consent for the creation flow during
   * onboarding. If the `userPref` is not supported the returned promise will be
   * rejected.
   * @param {string} userPref
   * @return {!Promise<Object>}
   */
  getUserPref(userPref) {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'getUserPref', [userPref]);
  },

  /**
   * Returns consent given by the user to enable creation flow during
   * onboarding.
   * @param {string} userPref
   * @param {Object} value A preference can store multiple types (dictionaries,
   *     lists, Boolean, etc..); therefore, accept a generic Object value.
   * @return {!Promise} Promise resolved when the request was handled.
   */
  setUserPref(userPref, value) {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'setUserPref', [userPref, value]);
  },

  /**
   * Triggers the opening of the Chrome feedback dialog.
   * @return {!Promise}
   */
  openFeedbackDialog() {
    return AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
        'openFeedbackDialog', []);
  },

  /**
   * Gets information about the specified video from DriveFS.
   * @param {string} videoFileId The Drive item id of the video file.
   * @param {string|undefined} resourceKey The Drive item resource key.
   * TODO(b/237089852): Wire up the resource key once DriveFS has support.
   * @return {!Promise<!projectorApp.Video>}
   */
  async getVideo(videoFileId, resourceKey) {
    try {
      const video =
          await AppUntrustedCommFactory.getPostMessageAPIClient().callApiFn(
              'getVideo', [videoFileId, resourceKey]);
      const videoFile = await getOrCreateLoadFilePromise(videoFileId).promise;
      // The streaming url must be generated in the untrusted context.
      // The corresponding cleanup call to URL.revokeObjectURL() happens in
      // ProjectorViewer::maybeResetUI() in Google3.
      video.srcUrl = URL.createObjectURL(videoFile);
      return video;
    } catch (e) {
      return Promise.reject(e);
    } finally {
      // Do not cache video files in the map because it's unclear when to
      // invalidate entries. Delete the key once we are finally done with it.
      loadingFiles.delete(videoFileId);
    }
  },
};

/**
 * Class that implements the RequestHandler inside the Projector untrusted
 * scheme for projector app.
 */
export class UntrustedAppRequestHandler extends RequestHandler {
  /**
   * @param {!Window} parentWindow  The embedder window from which requests
   *     come.
   */
  constructor(parentWindow) {
    super(null, TARGET_URL, TARGET_URL);
    this.targetWindow_ = parentWindow;

    this.registerMethod('onNewScreencastPreconditionChanged', (args) => {
      if (args.length !== 1) {
        console.error(
            'Invalid argument to onNewScreencastPreconditionChanged', args);
        return;
      }

      getAppElement().onNewScreencastPreconditionChanged(args[0]);
    });
    this.registerMethod('onSodaInstallProgressUpdated', (args) => {
      if (args.length !== 1 || isNaN(args[0])) {
        return;
      }

      getAppElement().onSodaInstallProgressUpdated(args[0]);
    });
    this.registerMethod('onSodaInstalled', (args) => {
      getAppElement().onSodaInstalled();
    });
    this.registerMethod('onSodaInstallError', (args) => {
      getAppElement().onSodaInstallError();
    });
    this.registerMethod('onScreencastsStateChange', (pendingScreencasts) => {
      getAppElement().onScreencastsStateChange(pendingScreencasts);
    });
    this.registerMethod('onFileLoaded', (args) => {
      if (args.length !== 3) {
        console.error('Invalid argument to onFileLoaded', args);
        return;
      }
      const fileId = args[0];
      const file = args[1];
      const error = args[2];
      const resolver = getOrCreateLoadFilePromise(fileId);
      if (!file || error) {
        resolver.reject(error);
        return;
      }
      resolver.resolve(file);
    });
  }

  /** @override */
  targetWindow() {
    return this.targetWindow_;
  }
}

/**
 * This is a class that is used to setup the duplex communication channels
 * between this origin, chrome-untrusted://projector/* and the embedder content.
 */
export class AppUntrustedCommFactory {
  /**
   * Creates the instances of PostMessageAPIClient and Requesthandler.
   */
  static maybeCreateInstances() {
    if (AppUntrustedCommFactory.client_ ||
        AppUntrustedCommFactory.requestHandler_) {
      return;
    }

    AppUntrustedCommFactory.client_ =
        new PostMessageAPIClient(TARGET_URL, window.parent);

    AppUntrustedCommFactory.requestHandler_ =
        new UntrustedAppRequestHandler(window.parent);

    getAppElement().setClientDelegate(CLIENT_DELEGATE);
  }

  /**
   * In order to use this class, please do the following (e.g. to check if it is
   * possible to start a new ProjectorSession):
   * const canStart = await AppUntrustedCommFactory. getPostMessageAPIClient().
   *     canStartProjectorSession();
   * @return {!PostMessageAPIClient}
   */
  static getPostMessageAPIClient() {
    // .AppUntrustedCommFactory.client_ should be available. However to be on
    // the cautious side create an instance here if getPostMessageAPIClient is
    // triggered before the page finishes loading.
    AppUntrustedCommFactory.maybeCreateInstances();
    return AppUntrustedCommFactory.client_;
  }
}

waitForAppElement().then(() => {
  // Create instances of the singletons (PostMessageAPIClient and
  // RequestHandler) when the document has finished loading.
  AppUntrustedCommFactory.maybeCreateInstances();
});
