// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-contact-visibility' component allows users to
 * set the preferred visibility to contacts for Nearby Share. This component is
 * embedded in the nearby_visibility_page as well as the settings pop-up dialog.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_card_radio_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './nearby_page_template.js';
import './nearby_shared_icons.html.js';

import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getContactManager, observeContactManager} from './nearby_contact_manager.js';
import {getTemplate} from './nearby_contact_visibility.html.js';
import {NearbySettings} from './nearby_share_settings_behavior.js';

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
 * @type {string}
 */
const DEVICE_VISIBILITY_LIGHT_ICON =
    'nearby-images:nearby-device-visibility-light';

/**
 * @type {string}
 */
const DEVICE_VISIBILITY_DARK_ICON =
    'nearby-images:nearby-device-visibility-dark';

/**
 * @typedef {{
 *            id:string,
 *            name:string,
 *            description:string,
 *            checked:boolean,
 *          }}
 */
export let NearbyVisibilityContact;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NearbyContactVisibilityElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NearbyContactVisibilityElement extends
    NearbyContactVisibilityElementBase {
  static get is() {
    return 'nearby-contact-visibility';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

      /** @type {?NearbySettings} */
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
       * @type {?Array<NearbyVisibilityContact>} The user's contacts
       *     re-formatted for binding.
       */
      contacts: {
        type: Array,
        value: null,
      },

      /** @private */
      numUnreachable_: {
        type: Number,
        value: 0,
        observer: 'updateNumUnreachableMessage_',
      },

      /** @private */
      numUnreachableMessage_: {
        type: String,
        value: '',
        notify: true,
      },

      isVisibilitySelected: {
        type: Boolean,
        computed: 'isVisibilitySelected_(selectedVisibility)',
        notify: true,
      },

      /**
       * Whether the contact visibility page is being rendered in dark mode.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'settingsChanged_(settings.visibility)',
    ];
  }

  constructor() {
    super();
    /** @private {?nearbyShare.mojom.ContactManagerInterface} */
    this.contactManager_ = null;

    /** @private {?nearbyShare.mojom.DownloadContactsObserverReceiver} */
    this.downloadContactsObserverReceiver_ = null;

    /** @private {?number} */
    this.downloadTimeoutId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.contactManager_ = getContactManager();
    this.downloadContactsObserverReceiver_ = observeContactManager(
        /** @type {!nearbyShare.mojom.DownloadContactsObserverInterface} */ (
            this));
    // Start a contacts download now so we have it by the time the component is
    // shown.
    this.downloadContacts_();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    if (this.downloadContactsObserverReceiver_) {
      this.downloadContactsObserverReceiver_.$.close();
    }
  }

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @param {?string} selectedVisibility
   * @return {boolean} Returns true when the current selectedVisibility has a
   *     value other than null.
   * @private
   */
  isVisibilitySelected_(selectedVisibility) {
    return this.selectedVisibility !== null;
  }

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
  }

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
  }

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
  }

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
   * @param {number} numUnreachableExcluded the number of contact records that
   *     are not reachable for Nearby Share. These are not included in the list
   *     of contact records.
   */
  onContactsDownloaded(
      allowedContacts, contactRecords, numUnreachableExcluded) {
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
    this.numUnreachable_ = numUnreachableExcluded;
  }

  /**
   * From nearbyShare.mojom.DownloadContactsObserver, called when contacts have
   * failed to download or the local timeout has triggered.
   */
  onContactsDownloadFailed() {
    this.contactsState = ContactsState.FAILED;
    clearTimeout(this.downloadTimeoutId_);
  }

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @return {boolean} returns true when checkboxes should be shown for
   *     contacts.
   * @private
   */
  showContactCheckBoxes_() {
    return this.selectedVisibility === 'some' ||
        this.selectedVisibility === 'none';
  }

  /**
   * When the contact check boxes are visible, the contact name and description
   * can be aria-hidden since they are used as labels for the checkbox.
   * @return {string|undefined} Whether the contact name and description should
   *     be aria-hidden. "true" or undefined.
   * @private
   */
  getContactAriaHidden_() {
    if (this.showContactCheckBoxes_()) {
      return 'true';
    }
    return undefined;
  }

  /**
   * Used to show/hide ui elements based on the contacts download state.
   * @param {string} contactsState
   * @param {string} expectedState
   * @return {boolean} true when in the expected state
   * @private
   */
  inContactsState_(contactsState, expectedState) {
    return contactsState === expectedState;
  }

  /**
   * @param {?NearbySettings} settings
   * @private
   */
  settingsChanged_(settings) {
    if (this.settings && this.settings.visibility) {
      this.selectedVisibility =
          visibilityValueToString(this.settings.visibility);
    } else {
      this.selectedVisibility = null;
    }
  }

  /**
   * @param {string} contactsState
   * @return {boolean} true when the radio group should be disabled
   * @private
   */
  disableRadioGroup_(contactsState) {
    return contactsState === ContactsState.PENDING ||
        contactsState === ContactsState.FAILED;
  }

  /**
   * @param {string} selectedVisibility
   * @param {string} contactsState
   * @return {boolean} true when zero state should be shown
   * @private
   */
  showZeroState_(selectedVisibility, contactsState) {
    return !selectedVisibility && contactsState !== ContactsState.PENDING &&
        contactsState !== ContactsState.FAILED;
  }

  /**
   * @param {string} selectedVisibility
   * @param {string} contactsState
   * @return {boolean} true when explanation container should be shown
   * @private
   */
  showContactsContainer_(selectedVisibility, contactsState) {
    return this.showExplanationState_(selectedVisibility, contactsState) ||
        this.showContactList_(selectedVisibility, contactsState);
  }

  /**
   * @param {string} selectedVisibility
   * @param {string} contactsState
   * @return {boolean} true when explanation state should be shown
   * @private
   */
  showExplanationState_(selectedVisibility, contactsState) {
    if (!selectedVisibility || contactsState === ContactsState.PENDING ||
        contactsState === ContactsState.FAILED) {
      return false;
    }

    return selectedVisibility === 'none' ||
        contactsState === ContactsState.HAS_CONTACTS;
  }

  /**
   * @param {string} selectedVisibility
   * @param {string} contactsState
   * @return {boolean} true when empty state should be shown
   * @private
   */
  showEmptyState_(selectedVisibility, contactsState) {
    return (selectedVisibility === 'all' || selectedVisibility === 'some') &&
        contactsState === ContactsState.ZERO_CONTACTS;
  }

  /**
   * @param {string} selectedVisibility
   * @param {string} contactsState
   * @return {boolean} true when contact list should be shown
   * @private
   */
  showContactList_(selectedVisibility, contactsState) {
    return (selectedVisibility === 'all' || selectedVisibility === 'some') &&
        contactsState === ContactsState.HAS_CONTACTS;
  }

  /**
   * Builds the html for the download retry message, applying the appropriate
   * aria labels, and adding an event listener to the link. This function is
   * largely copied from getAriaLabelledHelpText_ in <nearby-discovery-page>,
   * and should be generalized in the future. We do this here because the div
   * doesn't exist when the dialog loads, only once the template is added to the
   * DOM.
   * TODO(crbug.com/1170849): Extract this logic into a general method.
   *
   * @private
   */
  domChangeDownloadFailed_() {
    const contactsFailedMessage =
        this.shadowRoot.querySelector('#contactsFailedMessage');
    if (!contactsFailedMessage) {
      return;
    }
    const localizedString =
        this.i18nAdvanced('nearbyShareContactVisibilityDownloadFailed');
    contactsFailedMessage.innerHTML = localizedString;

    const ariaLabelledByIds = [];
    contactsFailedMessage.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType === Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `contactsFailedMessage${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', true);
        node.replaceWith(spanNode);
        return;
      }
      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (node.nodeType === Node.ELEMENT_NODE && node.nodeName === 'A') {
        node.id = `tryAgainLink`;
        ariaLabelledByIds.push(node.id);
        return;
      }

      // Only text and <a> nodes are allowed.
      assertNotReached(
          'nearbyShareContactVisibilityDownloadFailed has invalid node types');
    });

    const anchorTags = contactsFailedMessage.getElementsByTagName('a');
    // In the event the localizedString contains only text nodes, populate the
    // contents with the localizedString.
    if (anchorTags.length === 0) {
      contactsFailedMessage.innerHTML = localizedString;
      return;
    }

    assert(
        anchorTags.length === 1,
        'string should contain exactly one anchor tag');
    const anchorTag = anchorTags[0];
    anchorTag.setAttribute('aria-labelledby', ariaLabelledByIds.join(' '));
    anchorTag.href = '#';

    anchorTag.addEventListener('click', event => {
      event.preventDefault();
      this.downloadContacts_();
    });
  }

  /**
   * Builds the html for the zero state help text, applying the appropriate aria
   * labels, and setting the href of the link. This function is largely copied
   * from getAriaLabelledContent_ in <localized-link>, which can't be
   * used directly because this is Polymer element is used outside settings.
   * TODO(crbug.com/1170849): Extract this logic into a general method.
   * @return {string}
   * @private
   */
  getAriaLabelledZeroStateText_() {
    const tempEl = document.createElement('div');
    const localizedString =
        this.i18nAdvanced('nearbyShareContactVisibilityZeroStateText');
    const linkUrl = this.i18n('nearbyShareLearnMoreLink');
    tempEl.innerHTML = localizedString;

    const ariaLabelledByIds = [];
    tempEl.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType === Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `zeroStateText${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', true);
        node.replaceWith(spanNode);
        return;
      }
      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (node.nodeType === Node.ELEMENT_NODE && node.nodeName === 'A') {
        node.id = `zeroStateHelpLink`;
        ariaLabelledByIds.push(node.id);
        return;
      }

      // Only text and <a> nodes are allowed.
      assertNotReached(
          'nearbyShareContactVisibilityZeroStateText has invalid node types');
    });

    const anchorTags = tempEl.getElementsByTagName('a');
    // In the event the localizedString contains only text nodes, populate the
    // contents with the localizedString.
    if (anchorTags.length === 0) {
      return localizedString;
    }

    assert(
        anchorTags.length === 1,
        'nearbyShareContactVisibilityZeroStateText should contain exactly' +
            ' one anchor tag');
    const anchorTag = anchorTags[0];
    anchorTag.setAttribute('aria-labelledby', ariaLabelledByIds.join(' '));
    anchorTag.href = linkUrl;
    anchorTag.target = '_blank';

    return tempEl.innerHTML;
  }

  /**
   * @return {boolean} true if the unreachable contacts message should be shown
   * @private
   */
  showUnreachableContactsMessage_() {
    return this.numUnreachable_ > 0;
  }

  /** @private */
  updateNumUnreachableMessage_() {
    if (this.numUnreachable_ === 0) {
      this.numUnreachableMessage_ = '';
      return;
    }

    // Template: "# contacts are not available." with correct plural of
    // "contact".
    const labelTemplate =
        sendWithPromise(
            'getPluralString', 'nearbyShareContactVisibilityNumUnreachable',
            this.numUnreachable_)
            .then((labelTemplate) => {
              this.numUnreachableMessage_ = loadTimeData.substituteString(
                  labelTemplate, this.numUnreachable_);
            });
  }

  /**
   * @param {string} selectedVisibility
   * @return {string}
   * @private
   */
  getVisibilityDescription_(selectedVisibility) {
    switch (visibilityStringToValue(selectedVisibility)) {
      case nearbyShare.mojom.Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityOwnAll');
      case nearbyShare.mojom.Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilityOwnSome');
      case nearbyShare.mojom.Visibility.kNoOne:
        return this.i18nAdvanced('nearbyShareContactVisibilityOwnNone');
      default:
        return '';
    }
  }

  /**
   * Save visibility setting and sync allowed contacts with contact manager.
   * @public
   */
  saveVisibilityAndAllowedContacts() {
    const visibility = visibilityStringToValue(this.selectedVisibility);
    if (visibility) {
      this.set('settings.visibility', visibility);
    }

    const allowedContacts = [];
    if (this.contacts) {
      for (const contact of this.contacts) {
        if (contact.checked) {
          allowedContacts.push(contact.id);
        }
      }
    }
    this.contactManager_.setAllowedContacts(allowedContacts);
  }

  /**
   * Return the selected visibility as a enum to nearby_visibiity_page when
   * logging metric to avoid potential race condition
   *
   * @return {?nearbyShare.mojom.Visibility}
   *
   * @public
   */
  getSelectedVisibility() {
    return visibilityStringToValue(this.selectedVisibility);
  }

  /**
   * Returns the icon based on Light/Dark mode.
   * @returns {string}
   * @private
   */
  getDeviceVisibilityIcon_() {
    return this.isDarkModeActive_ ? DEVICE_VISIBILITY_DARK_ICON :
                                    DEVICE_VISIBILITY_LIGHT_ICON;
  }
}

customElements.define(
    NearbyContactVisibilityElement.is, NearbyContactVisibilityElement);
