// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-contact-visibility' component allows users to
 * set the preferred visibility to contacts for Nearby Share. This component is
 * embedded in the nearby_visibility_page as well as the settings pop-up dialog.
 */

'use strict';
(function() {

/** @enum {string} */
const ContactsState = {
  PENDING: 'pending',
  FAILED: 'failed',
  HAS_CONTACTS: 'hascontacts',
  ZERO_CONTACTS: 'zerocontacts',
};

/**
 * Maps visibility string to the mojo enum
 * @param {?string} visibilityString
 * @return {?nearbyShare.mojom.Visibility}
 */
const visibilityStringToValue = function(visibilityString) {
  switch (visibilityString) {
    case 'all':
      return nearbyShare.mojom.Visibility.kAllContacts;
    case 'some':
      return nearbyShare.mojom.Visibility.kSelectedContacts;
    case 'none':
      return nearbyShare.mojom.Visibility.kNoOne;
    default:
      return null;
  }
};

/**
 * Maps visibility mojo enum to a string for the radio button selection
 * @param {?nearbyShare.mojom.Visibility} visibility
 * @return {?string}
 */
const visibilityValueToString = function(visibility) {
  switch (visibility) {
    case nearbyShare.mojom.Visibility.kAllContacts:
      return 'all';
    case nearbyShare.mojom.Visibility.kSelectedContacts:
      return 'some';
    case nearbyShare.mojom.Visibility.kNoOne:
      return 'none';
    default:
      return null;
  }
};

/**
 * @typedef {{
 *            id:string,
 *            name:string,
 *            description:string,
 *            checked:boolean,
 *          }}
 */
/* #export */ let NearbyVisibilityContact;

Polymer({
  is: 'nearby-contact-visibility',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {Object} */
    ContactsState: {
      type: Object,
      value: ContactsState,
    },

    /** @type {string} */
    contactsState: {
      type: String,
      value: ContactsState.PENDING,
    },

    /** @type {?nearby_share.NearbySettings} */
    settings: {
      type: Object,
      notify: true,
    },

    /**
     * @type {?string} Which of visibility setting is selected as a string or
     *      null for no selection. ('all', 'some', 'none', null).
     */
    selectedVisibility: {
      type: String,
      value: null,
      notify: true,
    },

    /**
     * @type {?Array<NearbyVisibilityContact>} The user's contacts re-formatted
     *     for binding.
     */
    contacts: {
      type: Array,
      value: null,
    },
  },

  /** @private {?nearbyShare.mojom.ContactManagerInterface} */
  contactManager_: null,

  /** @private {?nearbyShare.mojom.DownloadContactsObserverReceiver} */
  downloadContactsObserverReceiver_: null,

  /** @private {?number} */
  downloadTimeoutId_: null,

  observers: [
    'settingsChanged_(settings.visibility)',
    'selectedVisibilityChanged_(selectedVisibility)',
  ],

  /** @type {ResizeObserver} used to observer size changes to this element */
  resizeObserver_: null,

  /** @override */
  attached() {
    // This is a required work around to get the iron-list to display on first
    // view. Currently iron-list won't generate item elements on attach if the
    // element is not visible. Because we are hosted in a cr-view-manager for
    // on-boarding, this component is not visible when the items are bound. To
    // fix this issue, we listen for resize events (which happen when display is
    // switched from none to block by the view manager) and manually call
    // notifyResize on the iron-list
    this.resizeObserver_ = new ResizeObserver(entries => {
      const contactList =
          /** @type {IronListElement} */ (this.$$('#contactList'));
      if (contactList) {
        contactList.notifyResize();
      }
    });
    this.resizeObserver_.observe(this);
    this.contactManager_ = nearby_share.getContactManager();
    this.downloadContactsObserverReceiver_ = nearby_share.observeContactManager(
        /** @type {!nearbyShare.mojom.DownloadContactsObserverInterface} */ (
            this));
    // Start a contacts download now so we have it by the time the component is
    // shown.
    this.downloadContacts_();
  },

  /** @override */
  detached() {
    this.resizeObserver_.disconnect();

    if (this.downloadContactsObserverReceiver_) {
      this.downloadContactsObserverReceiver_.$.close();
    }
  },

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @param {?string} selectedVisibility
   * @return {boolean} Returns true when the current selectedVisibility has a
   *     value other than null.
   * @private
   */
  isVisibilitySelected_(selectedVisibility) {
    return this.selectedVisibility !== null;
  },

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @param {?string} selectedVisibility
   * @param {string} visibilityString
   * @return {boolean} returns true when the current selectedVisibility equals
   *     the passed arguments.
   * @private
   */
  isVisibility_(selectedVisibility, visibilityString) {
    return this.selectedVisibility === visibilityString;
  },

  /**
   * Makes a mojo request to download the latest version of contacts.
   * @private
   */
  downloadContacts_() {
    // Don't show pending UI if we already have some contacts.
    if (!this.contacts) {
      this.contactsState = ContactsState.PENDING;
    }
    this.contactManager_.downloadContacts();
    // Time out after 30 seconds and show the failure page if we don't get a
    // response.
    this.downloadTimeoutId_ =
        setTimeout(this.onContactsDownloadFailed.bind(this), 30 * 1000);
  },

  /**
   * @param {boolean} allowed
   * @param {!nearbyShare.mojom.ContactRecord} contactRecord
   * @return {!NearbyVisibilityContact}
   * @private
   */
  toNearbyVisibilityContact_(allowed, contactRecord) {
    // TODO (vecore): Support multiple identifiers with template string.
    // We should always have at least 1 identifier.
    assert(contactRecord.identifiers.length > 0);

    /** @type {string} */
    const description = contactRecord.identifiers[0].accountName ||
        contactRecord.identifiers[0].phoneNumber || '';

    return {
      id: contactRecord.id,
      description: description,
      name: contactRecord.personName,
      checked: allowed,
    };
  },

  /**
   * From nearbyShare.mojom.DownloadContactsObserver, called when contacts have
   * been successfully downloaded.
   * @param {!Array<!string>} allowedContacts the server ids of the contacts
   *     that are allowed to see this device when visibility is
   *     kSelectedContacts. This corresponds to the checkbox shown next to the
   *     contact for kSelectedContacts visibility.
   * @param {!Array<!nearbyShare.mojom.ContactRecord>} contactRecords the full
   *     set of contacts returned from the people api. All contacts are shown to
   *     the user so they can see who can see their device for visibility
   *     kAllContacts and so they can choose which contacts are
   *     |allowedContacts| for kSelectedContacts visibility.
   */
  onContactsDownloaded(allowedContacts, contactRecords) {
    clearTimeout(this.downloadTimeoutId_);

    // TODO(vecore): Do a smart two-way merge that only splices actual changes.
    // For now we will do simple regenerate and bind.
    const allowed = new Set(allowedContacts);
    const items = [];
    for (const contact of contactRecords) {
      const visibilityContact =
          this.toNearbyVisibilityContact_(allowed.has(contact.id), contact);
      items.push(visibilityContact);
    }
    this.contacts = items;
    this.contactsState = items.length > 0 ? ContactsState.HAS_CONTACTS :
                                            ContactsState.ZERO_CONTACTS;
  },

  /**
   * From nearbyShare.mojom.DownloadContactsObserver, called when contacts have
   * failed to download or the local timeout has triggered.
   */
  onContactsDownloadFailed() {
    this.contactsState = ContactsState.FAILED;
    clearTimeout(this.downloadTimeoutId_);
  },

  /**
   * Sync the latest contact toggle states and update allowedContacts through
   * the contact manager.
   * @private
   */
  syncContactToggleState_() {
    const allowedContacts = [];
    if (this.contacts) {
      for (const contact of this.contacts) {
        if (contact.checked) {
          allowedContacts.push(contact.id);
        }
      }
    }
    this.contactManager_.setAllowedContacts(allowedContacts);
  },

  /**
   * TODO(crbug.com/1128256): Remove after specs/a11y.
   * Call from the JS debug console to test scrolling.
   * @param {number} numContacts
   * @private
   */
  genFakeContacts_(numContacts) {
    clearTimeout(this.downloadTimeoutId_);
    const fakeContacts = [];
    for (let i = 0; i < numContacts; i++) {
      fakeContacts.push({
        id: String(-i),
        description: String(i) + '@google.com',
        name: 'Person ' + i,
        checked: false,
      });
    }
    this.contacts = fakeContacts;
    this.contactsState = ContactsState.HAS_CONTACTS;
  },

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @param {?string} selectedVisibility
   * @return {boolean} returns true when checkboxes should be shown for
   *     contacts.
   * @private
   */
  showContactCheckBoxes_(selectedVisibility) {
    return this.selectedVisibility === 'some' ||
        this.selectedVisibility === 'none';
  },

  /**
   * Used to show/hide ui elements based on the contacts download state.
   * @param {string} contactsState
   * @param {string} expectedState
   * @return {boolean} true when in the expected state
   * @private
   */
  inContactsState_(contactsState, expectedState) {
    return contactsState === expectedState;
  },

  /**
   * @param {?nearby_share.NearbySettings} settings
   * @private
   */
  settingsChanged_(settings) {
    if (this.settings && this.settings.visibility) {
      this.selectedVisibility =
          visibilityValueToString(this.settings.visibility);
    } else {
      this.selectedVisibility = null;
    }
  },

  /**
   * @param {string} selectedVisibility
   * @private
   */
  selectedVisibilityChanged_(selectedVisibility) {
    const visibility = visibilityStringToValue(this.selectedVisibility);
    if (visibility) {
      this.set('settings.visibility', visibility);
    }
  },
});
})();
