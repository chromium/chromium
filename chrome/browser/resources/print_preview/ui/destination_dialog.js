// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../print_preview_utils.js';
import './destination_list.js';
import './print_preview_search_box.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import './provisional_destination_resolver.js';
import '../strings.m.js';
import './throbber_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {beforeNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {DestinationStore} from '../data/destination_store.js';
import {Invitation} from '../data/invitation.js';
import {InvitationStore} from '../data/invitation_store.js';
import {Metrics, MetricsContext} from '../metrics.js';
import {NativeLayerImpl} from '../native_layer.js';

Polymer({
  is: 'print-preview-destination-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, ListPropertyUpdateBehavior],

  properties: {
    /** @type {?DestinationStore} */
    destinationStore: {
      type: Object,
      observer: 'onDestinationStoreSet_',
    },

    /** @type {?InvitationStore} */
    invitationStore: {
      type: Object,
      observer: 'onInvitationStoreSet_',
    },

    activeUser: {
      type: String,
      observer: 'onActiveUserChange_',
    },

    currentDestinationAccount: String,

    /** @type {!Array<string>} */
    users: Array,

    /** @private {?Invitation} */
    invitation_: {
      type: Object,
      value: null,
    },

    /** @private {!Array<!Destination>} */
    destinations_: {
      type: Array,
      value: [],
    },

    /** @private {boolean} */
    loadingDestinations_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showCloudPrintWarning_: {
      type: Boolean,
      computed: 'computeShowCloudPrintWarning_(destinations_.splices)',
      value: false,
    },

    /** @private {?RegExp} */
    searchQuery_: {
      type: Object,
      value: null,
    },

    // <if expr="chromeos">
    /** @private */
    saveToDriveFlagEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('printSaveToDrive');
      },
      readOnly: true,
    },
    // </if>
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private {!MetricsContext} */
  metrics_: MetricsContext.destinationSearch(),

  // <if expr="chromeos">
  /** @private {?Destination} */
  destinationInConfiguring_: null,
  // </if>

  /** @private {boolean} */
  initialized_: false,

  /** @override */
  detached() {
    this.tracker_.removeAll();
  },

  /**
   * @return {boolean} Whether the destinations dialog should show a Cloud Print
   *     deprecation warning.
   * @private
   */
  computeShowCloudPrintWarning_() {
    return this.destinations_.some(destination => {
      return destination.shouldShowSaveToDriveWarning ||
          destination.shouldShowDeprecatedPrinterWarning;
    });
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_(e) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    if (e.key === 'Escape' &&
        (e.composedPath()[0] !== searchInput || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
    }
  },

  /** @private */
  onDestinationStoreSet_() {
    assert(this.destinations_.length === 0);
    const destinationStore = assert(this.destinationStore);
    this.tracker_.add(
        destinationStore, DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDestinations_.bind(this));
    this.tracker_.add(
        destinationStore, DestinationStore.EventType.DESTINATION_SEARCH_DONE,
        this.updateDestinationsAndInvitations_.bind(this));
    this.initialized_ = true;
  },

  /** @private */
  onInvitationStoreSet_() {
    const invitationStore = assert(this.invitationStore);
    this.tracker_.add(
        invitationStore, InvitationStore.EventType.INVITATION_SEARCH_DONE,
        this.updateInvitations_.bind(this));
    this.tracker_.add(
        invitationStore, InvitationStore.EventType.INVITATION_PROCESSED,
        this.updateInvitations_.bind(this));
  },

  /** @private */
  onActiveUserChange_() {
    if (this.activeUser) {
      this.$$('select').value = this.activeUser;
    }

    this.updateDestinationsAndInvitations_();
  },

  /** @private */
  updateDestinationsAndInvitations_() {
    if (!this.initialized_) {
      return;
    }

    this.updateDestinations_();
    if (this.activeUser && !!this.invitationStore) {
      this.invitationStore.startLoadingInvitations(this.activeUser);
    }
  },

  /** @private */
  updateDestinations_() {
    if (this.destinationStore === undefined) {
      return;
    }

    this.updateList(
        'destinations_', destination => destination.key,
        this.getDestinationList_());

    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
  },

  /**
   * @return {!Array<!Destination>}
   * @private
   */
  getDestinationList_() {
    const destinations = this.destinationStore.destinations(this.activeUser);
    // <if expr="chromeos">
    // When |saveToDriveFlagEnabled_| is true, we don't want to show a
    // 'Save to Drive' option in the destination dialog.
    if (this.saveToDriveFlagEnabled_) {
      return destinations.filter(
          destination => destination.id !== Destination.GooglePromotedId.DOCS &&
              destination.id !==
                  Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS);
    }
    // </if>

    return destinations;
  },

  /** @private */
  onCloseOrCancel_() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    const cancelled = this.$.dialog.getNative().returnValue !== 'success';
    this.metrics_.record(
        cancelled ?
            Metrics.DestinationSearchBucket.DESTINATION_CLOSED_UNCHANGED :
            Metrics.DestinationSearchBucket.DESTINATION_CLOSED_CHANGED);
    if (this.currentDestinationAccount &&
        this.currentDestinationAccount !== this.activeUser) {
      this.fire('account-change', this.currentDestinationAccount);
    }
  },

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.cancel();
  },

  /**
   * @param {!CustomEvent<!PrintPreviewDestinationListItemElement>} e Event
   *     containing the selected destination list item element.
   * @private
   */
  onDestinationSelected_(e) {
    const listItem = e.detail;
    const destination = listItem.destination;

    // ChromeOS local destinations that don't have capabilities need to be
    // configured before selecting, and provisional destinations need to be
    // resolved. Other destinations can be selected.
    if (destination.readyForSelection) {
      this.selectDestination_(destination);
      return;
    }

    // Provisional destinations
    if (destination.isProvisional) {
      this.$.provisionalResolver.resolveDestination(destination)
          .then(this.selectDestination_.bind(this))
          .catch(function() {
            console.warn(
                'Failed to resolve provisional destination: ' + destination.id);
          })
          .then(() => {
            if (this.$.dialog.open && listItem && !listItem.hidden) {
              listItem.focus();
            }
          });
      return;
    }

    // <if expr="chromeos">
    // Destination must be a CrOS local destination that needs to be set up.
    // The user is only allowed to set up printer at one time.
    if (this.destinationInConfiguring_) {
      return;
    }

    // Show the configuring status to the user and resolve the destination.
    listItem.onConfigureRequestAccepted();
    this.destinationInConfiguring_ = destination;
    this.destinationStore.resolveCrosDestination(destination)
        .then(
            response => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(response.success);
              if (response.success) {
                destination.capabilities = response.capabilities;
                if (response.policies) {
                  destination.policies = response.policies;
                }
                this.selectDestination_(destination);
                // <if expr="chromeos">
                // After destination is selected, start fetching for the EULA
                // URL.
                this.destinationStore.fetchEulaUrl(destination.id);
                // </if>
              }
            },
            () => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(false);
            });
    // </if>
  },

  /**
   * @param {!Destination} destination The destination to select.
   * @private
   */
  selectDestination_(destination) {
    this.destinationStore.selectDestination(destination);
    this.$.dialog.close();
  },

  show() {
    this.$.dialog.showModal();
    this.loadingDestinations_ = this.destinationStore === undefined ||
        this.destinationStore.isPrintDestinationSearchInProgress;
    this.metrics_.record(Metrics.DestinationSearchBucket.DESTINATION_SHOWN);
    if (this.activeUser) {
      beforeNextRender(assert(this.$$('select')), () => {
        this.$$('select').value = this.activeUser;
      });
    }
  },

  /** @return {boolean} Whether the dialog is open. */
  isOpen() {
    return this.$.dialog.hasAttribute('open');
  },

  /**
   * Updates printer sharing invitations UI.
   * @private
   */
  updateInvitations_() {
    const invitations = this.activeUser ?
        this.invitationStore.invitations(this.activeUser) :
        [];
    if (this.invitation_ !== invitations[0]) {
      this.metrics_.record(
          Metrics.DestinationSearchBucket.INVITATION_AVAILABLE);
    }
    this.invitation_ = invitations.length > 0 ? invitations[0] : null;
  },

  /**
   * @return {string} The text show show on the "accept" button in the
   *     invitation promo. 'Accept', 'Accept for group', or empty if there is no
   *     invitation.
   * @private
   */
  getAcceptButtonText_() {
    if (!this.invitation_) {
      return '';
    }

    return this.invitation_.asGroupManager ? this.i18n('acceptForGroup') :
                                             this.i18n('accept');
  },

  /**
   * @return {string} The formatted text to show for the invitation promo.
   * @private
   */
  getInvitationText_() {
    if (!this.invitation_) {
      return '';
    }

    if (this.invitation_.asGroupManager) {
      return this.i18nAdvanced('groupPrinterSharingInviteText', {
        substitutions: [
          this.invitation_.sender, this.invitation_.destination.displayName,
          this.invitation_.receiver
        ]
      });
    }

    return this.i18nAdvanced('printerSharingInviteText', {
      substitutions:
          [this.invitation_.sender, this.invitation_.destination.displayName]
    });
  },

  /** @private */
  onInvitationAcceptClick_() {
    this.metrics_.record(Metrics.DestinationSearchBucket.INVITATION_ACCEPTED);
    this.invitationStore.processInvitation(assert(this.invitation_), true);
    this.updateInvitations_();
  },

  /** @private */
  onInvitationRejectClick_() {
    this.metrics_.record(Metrics.DestinationSearchBucket.INVITATION_REJECTED);
    this.invitationStore.processInvitation(assert(this.invitation_), false);
    this.updateInvitations_();
  },

  /** @private */
  onUserChange_() {
    const select = this.$$('select');
    const account = select.value;
    if (account) {
      this.loadingDestinations_ = true;
      this.fire('account-change', account);
      this.metrics_.record(Metrics.DestinationSearchBucket.ACCOUNT_CHANGED);
    } else {
      select.value = this.activeUser;
      NativeLayerImpl.getInstance().signIn();
      this.metrics_.record(
          Metrics.DestinationSearchBucket.ADD_ACCOUNT_SELECTED);
    }
  },

  /** @private */
  onOpenSettingsPrintPage_() {
    this.metrics_.record(Metrics.DestinationSearchBucket.MANAGE_BUTTON_CLICKED);
    NativeLayerImpl.getInstance().openSettingsPrintPage();
  },
});
