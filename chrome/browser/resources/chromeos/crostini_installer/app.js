// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * Enum for the state of `crostini-installer-app`. Not to confused with
 * `installerState`.
 * @enum {string}
 */
const State = {
  PROMPT: 'prompt',
  INSTALLING: 'installing',
  ERROR: 'error',
  CANCELING: 'canceling',
};

const InstallerState = crostini.mojom.InstallerState;
const InstallerError = crostini.mojom.InstallerError;

Polymer({
  is: 'crostini-installer-app',

  _template: html`{__html_template__}`,

  properties: {
    /** @private */
    state_: {
      type: String,
      value: State.PROMPT,
    },

    /** @private */
    installerState_: {
      type: Number,
    },

    /** @private */
    installerProgress_: {
      type: Number,
    },

    /** @private */
    error_: {
      type: Number,
    },

    /**
     * Enable the html template to use State.
     * @private
     */
    State: {
      type: Object,
      value: State,
    },
  },

  /** @override */
  attached: function() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;

    this.listenerIds_ = [
      callbackRouter.onProgressUpdate.addListener(
          (installerState, progressFraction) => {
            this.installerState_ = installerState;
            this.installerProgress_ = progressFraction * 100;
          }),
      callbackRouter.onInstallFinished.addListener(error => {
        if (error === InstallerError.kNone) {
          // Install succeeded.
          this.closeDialog_();
        } else {
          assert(this.state_ === State.INSTALLING);
          this.error_ = error;
          this.state_ = State.ERROR;
        }
      }),
      callbackRouter.onCanceled.addListener(() => this.closeDialog_()),
    ];

    document.addEventListener('keyup', event => {
      if (event.key == 'Escape') {
        this.onCancelButtonClick_();
        event.preventDefault();
      }
    });

    this.$$('.action-button').focus();
  },

  /** @override */
  detached: function() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => callbackRouter.removeListener(id));
  },

  /** @private */
  onInstallButtonClick_: function() {
    assert(this.state_ === State.PROMPT || this.state_ === State.ERROR);
    this.installerState_ = InstallerState.kStart;
    this.installerProgress_ = 0;
    this.state_ = State.INSTALLING;
    BrowserProxy.getInstance().handler.install();
  },

  /** @private */
  onCancelButtonClick_: function() {
    switch (this.state_) {
      case State.PROMPT:
        BrowserProxy.getInstance().handler.cancelBeforeStart();
        this.closeDialog_();
        break;
      case State.INSTALLING:
        this.state_ = State.CANCELING;
        BrowserProxy.getInstance().handler.cancel();
        break;
      case State.ERROR:
        this.closeDialog_();
        break;
      case State.CANCELING:
        // Although cancel button has been disabled, we can reach here if users
        // press <esc> key.
        break;
      default:
        assertNotReached();
    }
  },

  /** @private */
  closeDialog_: function() {
    BrowserProxy.getInstance().handler.close();
  },

  /**
   * @param {State} state
   * @returns {string}
   * @private
   */
  getTitle_: function(state) {
    let titleId;
    switch (state) {
      case State.PROMPT:
        titleId = 'promptTitle';
        break;
      case State.INSTALLING:
        titleId = 'installingTitle';
        break;
      case State.ERROR:
        titleId = 'errorTitle';
        break;
      case State.CANCELING:
        titleId = 'cancelingTitle';
        break;
      default:
        assertNotReached();
    }
    return loadTimeData.getString(/** @type {string} */ (titleId));
  },

  /**
   * @param {State} state1
   * @param {State} state2
   * @returns {boolean}
   * @private
   */
  isState_: function(state1, state2) {
    return state1 === state2;
  },

  /**
   * @param {State} state
   * @returns {boolean}
   * @private
   */
  canInstall_: function(state) {
    return state === State.PROMPT || state === State.ERROR;
  },

  /**
   * @param {State} state
   * @returns {string}
   * @private
   */
  getInstallButtonLabel_: function(state) {
    switch (state) {
      case State.PROMPT:
        return loadTimeData.getString('install');
      case State.ERROR:
        return loadTimeData.getString('retry');
    }
    return '';
  },

  /**
   * @param {InstallerState} installerState
   * @returns {string}
   * @private
   */
  getProgressMessage_: function(installerState) {
    let messageId = null;
    switch (installerState) {
      case InstallerState.kStart:
        break;
      case InstallerState.kInstallImageLoader:
        messageId = 'loadTerminaMessage';
        break;
      case InstallerState.kStartConcierge:
        messageId = 'startConciergeMessage';
        break;
      case InstallerState.kCreateDiskImage:
        messageId = 'createDiskImageMessage';
        break;
      case InstallerState.kStartTerminaVm:
        messageId = 'startTerminaVmMessage';
        break;
      case InstallerState.kCreateContainer:
        // TODO(crbug.com/1015722): we are using the same message as for
        // |START_CONTAINER|, which is weird because user is going to see
        // message "start container" then "setup container" and then "start
        // container" again.
        messageId = 'startContainerMessage';
        break;
      case InstallerState.kSetupContainer:
        messageId = 'setupContainerMessage';
        break;
      case InstallerState.kStartContainer:
        messageId = 'startContainerMessage';
        break;
      case InstallerState.kFetchSshKeys:
        messageId = 'fetchSshKeysMessage';
        break;
      case InstallerState.kMountContainer:
        messageId = 'mountContainerMessage';
        break;
      default:
        assertNotReached();
    }

    return messageId ? loadTimeData.getString(messageId) : '';
  },

  /**
   * @param {InstallerError} error
   * @returns {string}
   * @private
   */
  getErrorMessage_: function(error) {
    let messageId = null;
    switch (error) {
      case InstallerError.kErrorLoadingTermina:
        messageId = 'loadTerminaError';
        break;
      case InstallerError.kErrorStartingConcierge:
        messageId = 'startConciergeError';
        break;
      case InstallerError.kErrorCreatingDiskImage:
        messageId = 'createDiskImageError';
        break;
      case InstallerError.kErrorStartingTermina:
        messageId = 'startTerminaVmError';
        break;
      case InstallerError.kErrorStartingContainer:
        messageId = 'startContainerError';
        break;
      case InstallerError.kErrorOffline:
        messageId = 'offlineError';
        break;
      case InstallerError.kErrorFetchingSshKeys:
        messageId = 'fetchSshKeysError';
        break;
      case InstallerError.kErrorMountingContainer:
        messageId = 'mountContainerError';
        break;
      case InstallerError.kErrorSettingUpContainer:
        messageId = 'setupContainerError';
        break;
      case InstallerError.kErrorInsufficientDiskSpace:
        messageId = 'insufficientDiskError';
        break;
      default:
        assertNotReached();
    }

    return messageId ? loadTimeData.getString(messageId) : '';
  },
});
