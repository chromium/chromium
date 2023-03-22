// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './contact_object.js';
import './shared_style.css.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './contact_tab.html.js';
import {NearbyContactBrowserProxy} from './nearby_contact_browser_proxy.js';
import {ContactUpdate} from './types.js';

Polymer({
  is: 'contact-tab',

  _template: getTemplate(),

  behaviors: [
    WebUIListenerBehavior,
  ],


  properties: {

    /** @private {!Array<!ContactUpdate>} */
    contactList_: {
      type: Array,
      value: [],
    },
  },

  /** @private {?NearbyContactBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = NearbyContactBrowserProxy.getInstance();
  },

  /**
   * When the page is initialized, notify the C++ layer to allow JavaScript and
   * initialize WebUI Listeners.
   * @override
   */
  attached() {
    this.addWebUIListener(
        'contacts-updated', contact => this.onContactUpdateAdded_(contact));
    this.browserProxy_.initialize();
  },

  /**
   * Downloads contacts from the Nearby Share server.
   * @private
   */
  onDownloadContacts() {
    this.browserProxy_.downloadContacts();
  },

  /**
   * Clears list of contact messages displayed.
   * @private
   */
  onClearMessagesButtonClicked_() {
    this.contactList_ = [];
  },

  /**
   * Adds contact sent in from WebUI listener to the list of displayed contacts.
   * @param {!ContactUpdate} contact
   * @private
   */
  onContactUpdateAdded_(contact) {
    this.unshift('contactList_', contact);
  },
});
