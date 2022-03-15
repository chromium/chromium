// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CloudPrintInterface, CloudPrintInterfaceErrorEventDetail, CloudPrintInterfaceEventType} from '../cloud_print_interface.js';
import {CloudPrintInterfaceImpl} from '../cloud_print_interface_impl.js';

import {DestinationOrigin} from './destination.js';
import {DestinationStore} from './destination_store.js';

type UpdateUsersPayload = {
  activeUser: string,
  users?: string[],
};

const PrintPreviewUserManagerElementBase = WebUIListenerMixin(PolymerElement);

export class PrintPreviewUserManagerElement extends
    PrintPreviewUserManagerElementBase {
  static get is() {
    return 'print-preview-user-manager';
  }

  static get properties() {
    return {
      activeUser: {
        type: String,
        notify: true,
      },

      cloudPrintDisabled: {
        type: Boolean,
        observer: 'onCloudPrintDisabledChanged_',
      },

      destinationStore: Object,

      users: {
        type: Array,
        notify: true,
        value() {
          return [];
        },
      },
    };
  }

  activeUser: string;
  cloudPrintDisabled: boolean;
  destinationStore: DestinationStore;
  users: string[];

  private cloudPrintInterface_: CloudPrintInterface|null = null;
  private initialized_: boolean = false;
  private tracker_: EventTracker = new EventTracker();

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.tracker_.removeAll();
    this.initialized_ = false;
  }

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
  }

  private onCloudPrintDisabledChanged_() {
    if (this.cloudPrintDisabled) {
      return;
    }

    this.cloudPrintInterface_ = CloudPrintInterfaceImpl.getInstance();
    this.tracker_.add(
        this.cloudPrintInterface_!.getEventTarget(),
        CloudPrintInterfaceEventType.UPDATE_USERS,
        (e: CustomEvent<UpdateUsersPayload>) =>
            this.onCloudPrintUpdateUsers_(e));
    [CloudPrintInterfaceEventType.SEARCH_FAILED,
     CloudPrintInterfaceEventType.PRINTER_FAILED,
    ].forEach(eventType => {
      this.tracker_.add(
          this.cloudPrintInterface_!.getEventTarget(), eventType,
          (e: CustomEvent<CloudPrintInterfaceErrorEventDetail>) =>
              this.checkCloudPrintStatus_(e));
    });
    if (this.users.length > 0) {
      this.cloudPrintInterface_!.setUsers(this.users);
    }
  }

  /**
   * Updates the cloud print status to NOT_SIGNED_IN if there is an
   * authentication error.
   */
  private checkCloudPrintStatus_(
      event: CustomEvent<CloudPrintInterfaceErrorEventDetail>) {
    if (event.detail.status !== 403 ||
        this.cloudPrintInterface_!.areCookieDestinationsDisabled()) {
      return;
    }

    // Should not have sent a message to Cloud Print if cloud print is
    // disabled.
    assert(!this.cloudPrintDisabled);
    this.updateActiveUser('');
    console.warn('Google Cloud Print Error: HTTP status 403');
  }

  /**
   * @param e Event containing the new active user and users.
   */
  private onCloudPrintUpdateUsers_(e: CustomEvent<UpdateUsersPayload>) {
    this.updateActiveUser(e.detail.activeUser);
    if (e.detail.users) {
      this.updateUsers_(e.detail.users);
    }
  }

  private updateUsers_(users: string[]) {
    const updateActiveUser = (users.length > 0 && this.users.length === 0) ||
        !users.includes(this.activeUser);
    this.users = users;
    if (this.cloudPrintInterface_!) {
      this.cloudPrintInterface_!.setUsers(users);
    }
    if (updateActiveUser) {
      this.updateActiveUser(this.users[0] || '');
    }
  }

  updateActiveUser(user: string) {
    if (user === this.activeUser) {
      return;
    }

    this.destinationStore.setActiveUser(user);
    this.activeUser = user;

    if (!user) {
      return;
    }

    this.destinationStore.reloadUserCookieBasedDestinations(user);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-user-manager': PrintPreviewUserManagerElement;
  }
}

customElements.define(
    PrintPreviewUserManagerElement.is, PrintPreviewUserManagerElement);
