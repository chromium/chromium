// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CloudPrintInterface, CloudPrintInterfaceErrorEventDetail, CloudPrintInterfaceEventType} from '../cloud_print_interface.js';
import {CloudPrintInterfaceImpl} from '../cloud_print_interface_impl.js';

import {Destination, DestinationOrigin} from './destination.js';
import {DestinationStore} from './destination_store.js';

/**
 * @typedef {{ activeUser: string,
 *             users: (!Array<string> | undefined) }}
 */
let UpdateUsersPayload;

Polymer({
  is: 'print-preview-user-manager',

  _template: null,

  behaviors: [WebUIListenerBehavior],

  properties: {
    activeUser: {
      type: String,
      notify: true,
    },

    cloudPrintDisabled: {
      type: Boolean,
      observer: 'onCloudPrintDisabledChanged_',
    },

    /** @type {?DestinationStore} */
    destinationStore: Object,

    /** @type {!Array<string>} */
    users: {
      type: Array,
      notify: true,
      value() {
        return [];
      },
    },
  },

  /** @private {?CloudPrintInterface} */
  cloudPrintInterface_: null,

  /** @private {boolean} */
  initialized_: false,

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  detached() {
    this.tracker_.removeAll();
    this.initialized_ = false;
  },

  initUserAccounts() {
    assert(!this.initialized_);
    this.initialized_ = true;

    if (this.cloudPrintDisabled) {
      return;
    }

    this.addWebUIListener('check-for-account-update', () => {
      this.destinationStore.startLoadCloudDestinations(
          DestinationOrigin.COOKIES);
    });
  },

  /** @private */
  onCloudPrintDisabledChanged_() {
    if (this.cloudPrintDisabled) {
      return;
    }

    this.cloudPrintInterface_ = CloudPrintInterfaceImpl.getInstance();
    this.tracker_.add(
        this.cloudPrintInterface_.getEventTarget(),
        CloudPrintInterfaceEventType.UPDATE_USERS,
        this.onCloudPrintUpdateUsers_.bind(this));
    [CloudPrintInterfaceEventType.SEARCH_FAILED,
     CloudPrintInterfaceEventType.PRINTER_FAILED,
    ].forEach(eventType => {
      this.tracker_.add(
          this.cloudPrintInterface_.getEventTarget(), eventType,
          this.checkCloudPrintStatus_.bind(this));
    });
    if (this.users.length > 0) {
      this.cloudPrintInterface_.setUsers(this.users);
    }
  },

  /**
   * Updates the cloud print status to NOT_SIGNED_IN if there is an
   * authentication error.
   * @param {!CustomEvent<!CloudPrintInterfaceErrorEventDetail>}
   *     event Contains the error status
   * @private
   */
  checkCloudPrintStatus_(event) {
    if (event.detail.status !== 403 ||
        this.cloudPrintInterface_.areCookieDestinationsDisabled()) {
      return;
    }

    // Should not have sent a message to Cloud Print if cloud print is
    // disabled.
    assert(!this.cloudPrintDisabled);
    this.updateActiveUser('');
    console.warn('Google Cloud Print Error: HTTP status 403');
  },

  /**
   * @param {!CustomEvent<!UpdateUsersPayload>} e Event containing the new
   *     active user and users.
   * @private
   */
  onCloudPrintUpdateUsers_(e) {
    this.updateActiveUser(e.detail.activeUser);
    if (e.detail.users) {
      this.updateUsers_(e.detail.users);
    }
  },

  /**
   * @param {!Array<string>} users The full list of signed in users.
   * @private
   */
  updateUsers_(users) {
    const updateActiveUser = (users.length > 0 && this.users.length === 0) ||
        !users.includes(this.activeUser);
    this.users = users;
    if (this.cloudPrintInterface_) {
      this.cloudPrintInterface_.setUsers(users);
    }
    if (updateActiveUser) {
      this.updateActiveUser(this.users[0] || '');
    }
  },

  /** @param {string} user The new active user. */
  updateActiveUser(user) {
    if (user === this.activeUser) {
      return;
    }

    this.destinationStore.setActiveUser(user);
    this.activeUser = user;

    if (!user) {
      return;
    }

    this.destinationStore.reloadUserCookieBasedDestinations(user);
  },
});
