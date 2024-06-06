// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-contact-visibility' component allows users to
 * set the preferred visibility to contacts for Nearby Share. This component is
 * embedded in the nearby_visibility_page as well as the settings pop-up dialog.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_card_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './nearby_page_template.js';
import './nearby_shared_icons.html.js';
// <if expr='chromeos_ash'>
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';

// </if>

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import type {ContactManagerInterface, ContactRecord, DownloadContactsObserverReceiver} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getContactManager, observeContactManager} from './nearby_contact_manager.js';
import {getTemplate} from './nearby_contact_visibility.html.js';
import type {NearbySettings} from './nearby_share_settings_mixin.js';

enum ContactsState {
  PENDING = 'pending',
  FAILED = 'failed',
  HAS_CONTACTS = 'hascontacts',
  ZERO_CONTACTS = 'zerocontacts',
}

function isHtmlAnchorElement(node: ChildNode): node is HTMLAnchorElement {
  return node.nodeType === Node.ELEMENT_NODE && node.nodeName === 'A';
}

const DEVICE_VISIBILITY_LIGHT_ICON =
    'nearby-images:nearby-device-visibility-light';

const DEVICE_VISIBILITY_DARK_ICON =
    'nearby-images:nearby-device-visibility-dark';

export interface NearbyVisibilityContact {
  id: string;
  name: string;
  description: string;
  checked: boolean;
}

const NearbyContactVisibilityElementBase = I18nMixin(PolymerElement);

export class NearbyContactVisibilityElement extends
    NearbyContactVisibilityElementBase {
  static get is() {
    return 'nearby-contact-visibility' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ContactsState: {
        type: Object,
        value: ContactsState,
      },

      contactsState: {
        type: String,
        value: ContactsState.PENDING,
      },

      settings: {
        type: Object,
        notify: true,
      },

      /**
       * Which visibility setting is selected as a string or
       * null for no selection: 'contacts', 'yourDevices', 'none'.
       */
      selectedVisibility: {
        type: String,
        value: null,
        notify: true,
      },

      /**
       * The user's contacts re-formatted for binding.
       */
      contacts: {
        type: Array,
        value: null,
      },

      numUnreachable_: {
        type: Number,
        value: 0,
        observer: 'updateNumUnreachableMessage_',
      },

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
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the user toggles All Contacts visibility.
       */
      isAllContactsToggledOn_: {
        type: Boolean,
        value() {
          return true;
        },
      },

      profileEmail: {
        type: String,
        value: '',
      },
    };
  }

  static get observers() {
    return [
      'settingsChanged_(settings.visibility)',
    ];
  }

  contacts: NearbyVisibilityContact[];
  // Mirroring the enum to allow usage in Polymer HTML bindings.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  ContactsState: ContactsState;
  contactsState: ContactsState;
  isVisibilitySelected: boolean;
  selectedVisibility: string|null;
  settings: NearbySettings|null;
  isSelectedContactsToggled: boolean;
  profileEmail: string;

  private contactManager_: ContactManagerInterface|null;
  private downloadContactsObserverReceiver_: DownloadContactsObserverReceiver|
      null;
  private downloadTimeoutId_: number|null;
  private isDarkModeActive_: boolean;
  private isAllContactsToggledOn_: boolean;
  private numUnreachable_: number;
  private numUnreachableMessage_: string;

  constructor() {
    super();
    this.contactManager_ = null;
    this.downloadContactsObserverReceiver_ = null;
    this.downloadTimeoutId_ = null;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.contactManager_ = getContactManager();
    this.downloadContactsObserverReceiver_ = observeContactManager(this);
    // Start a contacts download now so we have it by the time the component is
    // shown.
    this.downloadContacts_();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    if (this.downloadContactsObserverReceiver_) {
      this.downloadContactsObserverReceiver_.$.close();
    }
  }

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @return Returns true when the current selectedVisibility has a
   *     value other than null.
   */
  private isVisibilitySelected_(): boolean {
    return this.selectedVisibility !== null;
  }

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @return true when the current selectedVisibility equals
   *     the passed arguments.
   */
  private isVisibility_(
      selectedVisibility: string|null, visibilityString: string): boolean {
    return selectedVisibility === visibilityString;
  }

  /**
   * Maps visibility string to the mojo enum
   */
  private visibilityStringToValue(visibilityString: string|null): Visibility
      |null {
    switch (visibilityString) {
      case 'contacts':
        if (this.isAllContactsToggledOn_) {
          return Visibility.kAllContacts;
        }
        return Visibility.kSelectedContacts;
      case 'yourDevices':
        return Visibility.kYourDevices;
      case 'none':
        return Visibility.kNoOne;
      default:
        return null;
    }
  }

  /**
   * Maps visibility mojo enum to a string for the radio button selection
   */
  private visibilityValueToString(visibility: Visibility|null): string|null {
    switch (visibility) {
      case Visibility.kAllContacts:
        return 'contacts';
      case Visibility.kSelectedContacts:
        return 'contacts';
      case Visibility.kYourDevices:
        return 'yourDevices';
      case Visibility.kNoOne:
        return 'none';
      default:
        return null;
    }
  }

  /**
   * Makes a mojo request to download the latest version of contacts.
   */
  private downloadContacts_(): void {
    // Don't show pending UI if we already have some contacts.
    if (!this.contacts) {
      this.contactsState = ContactsState.PENDING;
    }
    this.contactManager_!.downloadContacts();
    // Time out after 30 seconds and show the failure page if we don't get a
    // response.
    this.downloadTimeoutId_ =
        setTimeout(this.onContactsDownloadFailed.bind(this), 30 * 1000);
  }

  private toNearbyVisibilityContact_(
      allowed: boolean, contactRecord: ContactRecord): NearbyVisibilityContact {
    // TODO (vecore): Support multiple identifiers with template string.
    // We should always have at least 1 identifier.
    assert(contactRecord.identifiers.length > 0);

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
   * From DownloadContactsObserver, called when contacts have
   * been successfully downloaded.
   * @param allowedContacts the server ids of the contacts
   *     that are allowed to see this device when visibility is
   *     kSelectedContacts. This corresponds to the checkbox shown next to the
   *     contact for kSelectedContacts visibility.
   * @param contactRecords the full
   *     set of contacts returned from the people api. All contacts are shown to
   *     the user so they can see who can see their device for visibility
   *     kAllContacts and so they can choose which contacts are
   *     |allowedContacts| for kSelectedContacts visibility.
   * @param numUnreachableExcluded the number of contact records that
   *     are not reachable for Nearby Share. These are not included in the list
   *     of contact records.
   */
  onContactsDownloaded(
      allowedContacts: string[], contactRecords: ContactRecord[],
      numUnreachableExcluded: number): void {
    clearTimeout(this.downloadTimeoutId_!);

    // TODO(vecore): Do a smart two-way merge that only splices actual changes.
    // For now we will do simple regenerate and bind.
    const allowed = new Set(allowedContacts);
    const items: NearbyVisibilityContact[] = [];
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
   * From DownloadContactsObserver, called when contacts have
   * failed to download or the local timeout has triggered.
   */
  onContactsDownloadFailed(): void {
    this.contactsState = ContactsState.FAILED;
    clearTimeout(this.downloadTimeoutId_!);
  }

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @return true when checkboxes should be shown for contacts.
   */
  private showContactCheckBoxes_(): boolean {
    return this.getSelectedVisibility() === Visibility.kSelectedContacts;
  }

  /**
   * When the contact check boxes are visible, the contact name and description
   * can be aria-hidden since they are used as labels for the checkbox.
   * @return Whether the contact name and description should
   *     be aria-hidden. "true" or undefined.
   */
  private getContactAriaHidden_(): string|undefined {
    if (this.showContactCheckBoxes_()) {
      return 'true';
    }
    return undefined;
  }

  /**
   * Used to show/hide ui elements based on the contacts download state.
   */
  private inContactsState_(contactsState: string, expectedState: string):
      boolean {
    return contactsState === expectedState;
  }

  private settingsChanged_(): void {
    if (this.settings && this.settings.visibility !== null) {
      this.selectedVisibility =
          this.visibilityValueToString(this.settings.visibility);
      this.isAllContactsToggledOn_ =
          this.settings.visibility === Visibility.kAllContacts;
    } else {
      this.selectedVisibility = null;
    }
  }

  private disableRadioGroup_(contactsState: string): boolean {
    return contactsState === ContactsState.PENDING ||
        contactsState === ContactsState.FAILED;
  }

  private showZeroState_(selectedVisibility: string, contactsState: string):
      boolean {
    return !selectedVisibility && contactsState !== ContactsState.PENDING &&
        contactsState !== ContactsState.FAILED;
  }

  private showContactsContainer_(
      selectedVisibility: string, contactsState: string): boolean {
    return this.showExplanationState_(selectedVisibility, contactsState) ||
        this.showContactList_(selectedVisibility, contactsState);
  }

  private showExplanationState_(
      selectedVisibility: string, contactsState: string): boolean {
    if (!selectedVisibility || contactsState === ContactsState.PENDING ||
        contactsState === ContactsState.FAILED) {
      return false;
    }

    return selectedVisibility === 'none' ||
        contactsState === ContactsState.HAS_CONTACTS;
  }

  private showEmptyState_(selectedVisibility: string, contactsState: string):
      boolean {
    return selectedVisibility === 'contacts' &&
        contactsState === ContactsState.ZERO_CONTACTS;
  }

  private showContactList_(selectedVisibility: string, contactsState: string):
      boolean {
    return selectedVisibility === 'contacts' &&
        contactsState === ContactsState.HAS_CONTACTS;
  }

  private showAllContactsToggle_(
      selectedVisibility: string, contactsState: ContactsState): boolean {
    return selectedVisibility === 'contacts' &&
        contactsState === ContactsState.HAS_CONTACTS;
  }

  private toggleAllContacts_(): void {
    this.isAllContactsToggledOn_ = !this.isAllContactsToggledOn_;
  }

  /**
   * Builds the html for the download retry message, applying the appropriate
   * aria labels, and adding an event listener to the link. This function is
   * largely copied from getAriaLabelledHelpText_ in <nearby-discovery-page>,
   * and should be generalized in the future. We do this here because the div
   * doesn't exist when the dialog loads, only once the template is added to the
   * DOM.
   * TODO(crbug.com/1170849): Extract this logic into a general method.
   */
  private domChangeDownloadFailed_(): void {
    const contactsFailedMessage =
        this.shadowRoot!.querySelector('#contactsFailedMessage');
    if (!contactsFailedMessage) {
      return;
    }
    const localizedString =
        this.i18nAdvanced('nearbyShareContactVisibilityDownloadFailed');
    contactsFailedMessage.innerHTML = localizedString;

    const ariaLabelledByIds: string[] = [];
    contactsFailedMessage.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType === Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `contactsFailedMessage${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', 'true');
        node.replaceWith(spanNode);
        return;
      }
      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (isHtmlAnchorElement(node)) {
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
   */
  private getAriaLabelledZeroStateText_(): TrustedHTML {
    const tempEl = document.createElement('div');
    const localizedString =
        this.i18nAdvanced('nearbyShareContactVisibilityZeroStateText');
    const linkUrl = this.i18n('nearbyShareLearnMoreLink');
    tempEl.innerHTML = localizedString;

    const ariaLabelledByIds: string[] = [];
    tempEl.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType === Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `zeroStateText${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', 'true');
        node.replaceWith(spanNode);
        return;
      }
      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (isHtmlAnchorElement(node)) {
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

    return sanitizeInnerHtml(
        tempEl.innerHTML, {attrs: ['id', 'aria-hidden', 'aria-labelledby']});
  }

  private showUnreachableContactsMessage_(): boolean {
    return this.numUnreachable_ > 0;
  }

  private updateNumUnreachableMessage_(): void {
    if (this.numUnreachable_ === 0) {
      this.numUnreachableMessage_ = '';
      return;
    }

    // Template: "# contacts are not available." with correct plural of
    // "contact".
    sendWithPromise(
        'getPluralString', 'nearbyShareContactVisibilityNumUnreachable',
        this.numUnreachable_)
        .then((labelTemplate) => {
          this.numUnreachableMessage_ = loadTimeData.substituteString(
              labelTemplate, this.numUnreachable_,
              this.i18n('nearbyShareFeatureName'));
        });
  }

  private getVisibilityDescription_(): TrustedHTML {
    switch (this.getSelectedVisibility()) {
      case Visibility.kAllContacts:
        return this.i18nAdvanced(
            'nearbyShareContactVisibilityOwnAllSelfShare',
            {substitutions: [this.profileEmail]});
      case Visibility.kSelectedContacts:
        return this.i18nAdvanced(
            'nearbyShareContactVisibilityOwnSomeSelfShare',
            {substitutions: [this.profileEmail]});
      case Visibility.kYourDevices:
        return this.i18nAdvanced(
            'nearbyShareContactVisibilityOwnYourDevices',
            {substitutions: [this.profileEmail]});
      case Visibility.kNoOne:
        return this.i18nAdvanced('nearbyShareContactVisibilityOwnNone');
      default:
        assert(window.trustedTypes);
        return window.trustedTypes.emptyHTML;
    }
  }

  /**
   * Save visibility setting and sync allowed contacts with contact manager.
   */
  saveVisibilityAndAllowedContacts(): void {
    const visibility = this.getSelectedVisibility();
    if (visibility) {
      this.set('settings.visibility', visibility);
    }

    if (!this.contacts) {
      this.contactManager_!.setAllowedContacts([]);
      return;
    }

    const allowedContacts: string[] = [];

    switch (visibility) {
      case Visibility.kAllContacts:
        for (const contact of this.contacts) {
          allowedContacts.push(contact.id);
        }
        break;
      case Visibility.kSelectedContacts:
        for (const contact of this.contacts) {
          if (contact.checked) {
            allowedContacts.push(contact.id);
          }
        }
        break;
      default:
        break;
    }
    this.contactManager_!.setAllowedContacts(allowedContacts);
  }

  /**
   * Return the selected visibility as a enum to nearby_visibiity_page when
   * logging metric to avoid potential race condition
   */
  getSelectedVisibility(): Visibility|null {
    return this.visibilityStringToValue(this.selectedVisibility);
  }

  /**
   * Returns the icon based on Light/Dark mode.
   */
  private getDeviceVisibilityIcon_(): string {
    return this.isDarkModeActive_ ? DEVICE_VISIBILITY_DARK_ICON :
                                    DEVICE_VISIBILITY_LIGHT_ICON;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyContactVisibilityElement.is]: NearbyContactVisibilityElement;
  }
}

customElements.define(
    NearbyContactVisibilityElement.is, NearbyContactVisibilityElement);
