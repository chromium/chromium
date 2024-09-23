// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {ProjectorError} from 'chrome-untrusted://projector/common/message_types.js';

import {installLaunchHandler} from './launch.js';
import {browserProxy} from './untrusted_projector_browser_proxy.js';

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
    return browserProxy.getAccounts();
  },

  /**
   * Checks whether the SWA can trigger a new Projector session.
   * @return {!Promise<!projectorApp.NewScreencastPreconditionState>}
   */
  getNewScreencastPreconditionState() {
    return browserProxy.getNewScreencastPreconditionState();
  },

  /**
   * Starts the Projector session if it is possible. Provides the storage dir
   * where  projector output artifacts will be saved in.
   * @param {string} storageDir
   * @return {Promise<boolean>}
   */
  startProjectorSession(storageDir) {
    return browserProxy.startProjectorSession(storageDir);
  },

  /**
   * Gets the oauth token with the required scopes for the specified account.
   * @param {string} account user's email
   * @return {!Promise<!projectorApp.OAuthToken>}
   */
  getOAuthTokenForAccount(account) {
    return Promise.reject('Unsupported method getOauthTokenForAccount');
  },

  /**
   * Sends 'error' message to handler.
   * The Handler will log the message. If the error is not a recoverable error,
   * the handler closes the corresponding WebUI.
   * @param {!Array<ProjectorError>} msg Error messages.
   */
  onError(msg) {
    console.error('Received error messages:', msg);
  },

  /**
   * Gets the list of pending screencasts that are uploading to drive on current
   * device.
   * @return {Promise<Array<projectorApp.PendingScreencast>>}
   */
  getPendingScreencasts() {
    return browserProxy.getPendingScreencasts();
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
    return browserProxy.sendXhr(
        url, method, requestBody, !!useCredentials, !!useApiKey, headers,
        accountEmail);
  },

  /**
   * Returns true if the device supports on device speech recognition.
   * @return {!Promise<boolean>}
   */
  shouldDownloadSoda() {
    return browserProxy.shouldDownloadSoda();
  },

  /**
   * Triggers the installation of on device speech recognition binary and
   * language packs for the user's locale. Returns true if download and
   * installation started.
   * @return {!Promise<boolean>}
   */
  installSoda() {
    return browserProxy.installSoda();
  },

  /**
   * Checks if the user has given consent for the creation flow during
   * onboarding. If the `userPref` is not supported the returned promise will be
   * rejected.
   * @param {string} userPref
   * @return {!Promise<Object>}
   */
  getUserPref(userPref) {
    return browserProxy.getUserPref(userPref);
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
    return browserProxy.setUserPref(userPref, value);
  },

  /**
   * Triggers the opening of the Chrome feedback dialog.
   * @return {!Promise}
   */
  openFeedbackDialog() {
    return browserProxy.openFeedbackDialog();
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
      const video = await browserProxy.getVideo(videoFileId, resourceKey);
      const videoFile = await getOrCreateLoadFilePromise(videoFileId).promise;
      return {
        fileId: videoFileId,
        durationMillis: video.durationMillis.toString(),
        // The streaming url must be generated in the untrusted context.
        // The corresponding cleanup call to URL.revokeObjectURL() happens in
        // ProjectorViewer::maybeResetUI() in Google3.
        srcUrl: URL.createObjectURL(videoFile),
      };
    } catch (e) {
      return Promise.reject(e);
    } finally {
      // Do not cache video files in the map because it's unclear when to
      // invalidate entries. Delete the key once we are finally done with it.
      loadingFiles.delete(videoFileId);
    }
  },
};


let initialized = false;
function initCommunication() {
  if (initialized) {
    return;
  }
  initialized = true;

  // Set the client delegate to the app element.
  getAppElement().setClientDelegate(CLIENT_DELEGATE);

  // Install launch handler to observe files sent from
  // Drivefs.
  installLaunchHandler((fileId, file, error) => {
    const resolver = getOrCreateLoadFilePromise(fileId);
    if (!file || error) {
      resolver.reject(error);
      return;
    }
    resolver.resolve(file);
  });

  const callbackRouter = browserProxy.getProjectorCallbackRouter();
  // Setup the callback routers to handle requests from browser process.
  callbackRouter.onNewScreencastPreconditionChanged.addListener(
      (precondition) => {
        try {
          getAppElement().onNewScreencastPreconditionChanged(precondition);
        } catch (error) {
          console.error(
              'Unable to notify onNewScreencastPreconditionChanged method',
              error);
        }
      });
  callbackRouter.onSodaInstallProgressUpdated.addListener((progress) => {
    getAppElement().onSodaInstallProgressUpdated(progress);
  });
  callbackRouter.onSodaInstallError.addListener(() => {
    getAppElement().onSodaInstallError();
  });
  callbackRouter.onSodaInstalled.addListener(() => {
    getAppElement().onSodaInstalled();
  });
  callbackRouter.onScreencastsStateChange.addListener((pendingScreencasts) => {
    getAppElement().onScreencastsStateChange(pendingScreencasts);
  });
}

/** @suppress {missingProperties} */
function initColorUpdater() {
  window.addEventListener('DOMContentLoaded', () => {
    // Start listening to color change events. These events get picked up by
    // logic in ts_helpers.ts on the google3 side.
    ColorChangeUpdater.forDocument().start();
  });

  // Expose functions to bind to color change events to window so they can be
  // automatically picked up by installColors(). See ts_helpers.ts in google3.
  window.addColorChangeListener = function(listener) {
    ColorChangeUpdater.forDocument().eventTarget.addEventListener(
        COLOR_PROVIDER_CHANGED, listener);
  };
  window.removeColorChangeListener = function(listener) {
    ColorChangeUpdater.forDocument().eventTarget.removeEventListener(
        COLOR_PROVIDER_CHANGED, listener);
  };
}

waitForAppElement().then(() => {
  // Create instances of the singletons (PostMessageAPIClient and
  // RequestHandler) when the document has finished loading.
  initCommunication();
  initColorUpdater();
});
