// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../data/user_manager.js';
import './destination_dialog.js';
import './destination_select.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import './throbber_css.js';
import './settings_section.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {beforeNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CloudPrintInterface} from '../cloud_print_interface.js';
import {createRecentDestinationKey, Destination, DestinationOrigin, makeRecentDestination, RecentDestination} from '../data/destination.js';
import {DestinationErrorType, DestinationStore} from '../data/destination_store.js';
import {InvitationStore} from '../data/invitation_store.js';
import {Error, State} from '../data/state.js';

import {SettingsBehavior} from './settings_behavior.js';

/** @enum {number} */
export const DestinationState = {
  INIT: 0,
  SELECTED: 1,
  SET: 2,
  UPDATED: 3,
  ERROR: 4,
};

/** @type {number} Number of recent destinations to save. */
const NUM_PERSISTED_DESTINATIONS = 3;

Polymer({
  is: 'print-preview-destination-settings',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    SettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {CloudPrintInterface} */
    cloudPrintInterface: {
      type: Object,
      observer: 'onCloudPrintInterfaceSet_',
    },

    dark: Boolean,

    /** @type {?Destination} */
    destination: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {!DestinationState} */
    destinationState: {
      type: Number,
      notify: true,
      value: DestinationState.INIT,
      observer: 'updateDestinationSelect_',
    },

    disabled: Boolean,

    /** @type {!Error} */
    error: {
      type: Number,
      notify: true,
      observer: 'onErrorChanged_',
    },

    firstLoad: Boolean,

    /** @type {!State} */
    state: Number,

    /** @private {string} */
    activeUser_: {
      type: String,
      observer: 'onActiveUserChanged_',
    },

    /** @private {boolean} */
    cloudPrintDisabled_: Boolean,

    /** @private {?DestinationStore} */
    destinationStore_: {
      type: Object,
      value: null,
    },

    /** @private {!Array<!RecentDestination>} */
    displayedDestinations_: Array,

    // <if expr="chromeos">
    hasPinSetting_: {
      type: Boolean,
      computed: 'computeHasPinSetting_(settings.pin.available)',
      reflectToAttribute: true,
    },
    // </if>

    /** @private {?InvitationStore} */
    invitationStore_: {
      type: Object,
      value: null,
    },

    /** @private {boolean} */
    isDialogOpen_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    noDestinations_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    pdfPrinterDisabled_: Boolean,

    /** @private */
    shouldHideSpinner_: {
      type: Boolean,
      computed: 'computeShouldHideSpinner_(destinationState, destination)',
    },

    /** @private {string} */
    statusText_: {
      type: String,
      computed: 'computeStatusText_(destination)',
    },

    /** @private {!Array<string>} */
    users_: Array,
  },

  /** @private {string} */
  lastUser_: '',

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  attached: function() {
    this.destinationStore_ =
        new DestinationStore(this.addWebUIListener.bind(this));
    this.invitationStore_ = new InvitationStore();
    this.tracker_.add(
        this.destinationStore_, DestinationStore.EventType.DESTINATION_SELECT,
        this.onDestinationSelect_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        this.onDestinationCapabilitiesReady_.bind(this));
    this.tracker_.add(
        this.destinationStore_, DestinationStore.EventType.ERROR,
        this.onDestinationError_.bind(this));
    // Need to update the recent list when the destination store inserts
    // destinations, in case any recent destinations have been added to the
    // store. At startup, recent destinations can be in the sticky settings,
    // but they should not be displayed in the dropdown until they have been
    // fetched by the DestinationStore, to ensure that they still exist.
    this.tracker_.add(
        assert(this.destinationStore_),
        DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDropdownDestinations_.bind(this));
  },

  /** @override */
  detached: function() {
    this.invitationStore_.resetTracker();
    this.destinationStore_.resetTracker();
    this.tracker_.removeAll();
  },

  /** @private */
  onCloudPrintInterfaceSet_: function() {
    const cloudPrintInterface = assert(this.cloudPrintInterface);
    this.destinationStore_.setCloudPrintInterface(cloudPrintInterface);
    this.invitationStore_.setCloudPrintInterface(cloudPrintInterface);
  },

  /** @private */
  onActiveUserChanged_: function() {
    // Re-filter the dropdown destinations for the new account.
    if (!this.isDialogOpen_) {
      // Don't update the destination settings UI while the dialog is open in
      // front of it.
      this.updateDropdownDestinations_();
    }

    if (!this.destination ||
        this.destination.origin !== DestinationOrigin.COOKIES) {
      // Active user changing doesn't impact non-cookie based destinations.
      return;
    }

    if (this.destination.account === this.activeUser_) {
      // If the current destination belongs to the new account and the dialog
      // was waiting for sign in, update the state.
      if (this.destinationState === DestinationState.SELECTED) {
        this.destinationState = this.destination.capabilities ?
            DestinationState.UPDATED :
            DestinationState.SET;
      }
      return;
    }

    if (this.isDialogOpen_) {
      // Do not update the selected destination if the dialog is open, as this
      // will change the destination settings UI behind the dialog, and the
      // user may be selecting a new destination in the dialog anyway. Wait
      // for the user to select a destination or cancel.
      return;
    }

    // Destination belongs to a different account. Reset the destination to
    // the most recent destination associated with the new account, or the
    // default.
    const recent = this.displayedDestinations_.find(d => {
      return d.origin !== DestinationOrigin.COOKIES ||
          d.account === this.activeUser_;
    });
    if (recent) {
      const success = this.destinationStore_.selectRecentDestinationByKey(
          createRecentDestinationKey(recent), this.displayedDestinations_);
      if (success) {
        return;
      }
    }
    this.destinationStore_.selectDefaultDestination();
  },

  /**
   * @param {string} defaultPrinter The system default printer ID.
   * @param {boolean} pdfPrinterDisabled Whether the PDF printer is disabled.
   * @param {string} serializedDefaultDestinationRulesStr String with rules
   *     for selecting a default destination.
   * @param {?Array<string>} userAccounts The signed in user accounts.
   * @param {boolean} syncAvailable Whether sync is available. Used to
   *     determine whether to wait for user info updates from the handler, or
   *     to always send requests to the Google Cloud Print server.
   */
  init: function(
      defaultPrinter, pdfPrinterDisabled, serializedDefaultDestinationRulesStr,
      userAccounts, syncAvailable) {
    this.pdfPrinterDisabled_ = pdfPrinterDisabled;
    this.$.userManager.initUserAccounts(userAccounts, syncAvailable);
    this.destinationStore_.init(
        this.pdfPrinterDisabled_, defaultPrinter,
        serializedDefaultDestinationRulesStr,
        /** @type {!Array<RecentDestination>} */
        (this.getSettingValue('recentDestinations')));
  },

  /** @private */
  onDestinationSelect_: function() {
    // If the user selected a destination in the dialog after changing the
    // active user, do the UI updates that were previously deferred.
    if (this.isDialogOpen_ && this.lastUser_ !== this.activeUser_) {
      this.updateDropdownDestinations_();
    }

    if (this.state === State.FATAL_ERROR) {
      // Don't let anything reset if there is a fatal error.
      return;
    }

    const destination = this.destinationStore_.selectedDestination;
    if (!!this.activeUser_ ||
        destination.origin !== DestinationOrigin.COOKIES) {
      this.destinationState = DestinationState.SET;
    } else {
      this.destinationState = DestinationState.SELECTED;
    }
    // Notify observers that the destination is set only after updating the
    // destinationState.
    this.destination = destination;
    this.updateRecentDestinations_();
  },

  /** @private */
  onDestinationCapabilitiesReady_: function() {
    this.notifyPath('destination.capabilities');
    this.updateRecentDestinations_();
    if (this.destinationState === DestinationState.SET) {
      this.destinationState = DestinationState.UPDATED;
    }
  },

  /**
   * @param {!CustomEvent<!DestinationErrorType>} e
   * @private
   */
  onDestinationError_: function(e) {
    let errorType = Error.NONE;
    switch (e.detail) {
      case DestinationErrorType.INVALID:
        errorType = Error.INVALID_PRINTER;
        break;
      case DestinationErrorType.UNSUPPORTED:
        errorType = Error.UNSUPPORTED_PRINTER;
        break;
      case DestinationErrorType.NO_DESTINATIONS:
        errorType = Error.NO_DESTINATIONS;
        this.noDestinations_ = true;
        break;
      default:
        break;
    }
    this.error = errorType;
  },

  /** @private */
  onErrorChanged_: function() {
    if (this.error == Error.INVALID_PRINTER ||
        this.error == Error.UNSUPPORTED_PRINTER ||
        this.error == Error.NO_DESTINATIONS) {
      this.destinationState = DestinationState.ERROR;
    }
  },

  /**
   * @param {!RecentDestination} destination
   * @return {boolean} Whether the destination is Save as PDF or Save to
   *     Drive.
   */
  destinationIsDriveOrPdf_: function(destination) {
    return destination.id === Destination.GooglePromotedId.SAVE_AS_PDF ||
        destination.id === Destination.GooglePromotedId.DOCS;
  },

  /** @private */
  updateRecentDestinations_: function() {
    if (!this.destination) {
      return;
    }

    // Determine if this destination is already in the recent destinations,
    // and where in the array it is located.
    const newDestination = makeRecentDestination(assert(this.destination));
    const recentDestinations =
        /** @type {!Array<!RecentDestination>} */ (
            this.getSettingValue('recentDestinations'));
    let indexFound = recentDestinations.findIndex(function(recent) {
      return (
          newDestination.id == recent.id &&
          newDestination.origin == recent.origin);
    });

    // No change
    if (indexFound == 0 &&
        recentDestinations[0].capabilities == newDestination.capabilities) {
      return;
    }
    const isNew = indexFound == -1;

    // Shift the array so that the nth most recent destination is located at
    // index n.
    if (isNew && recentDestinations.length == NUM_PERSISTED_DESTINATIONS) {
      indexFound = NUM_PERSISTED_DESTINATIONS - 1;
    }
    if (indexFound != -1) {
      this.setSettingSplice('recentDestinations', indexFound, 1, null);
    }

    // Add the most recent destination
    this.setSettingSplice('recentDestinations', 0, 0, newDestination);
    if (!this.destinationIsDriveOrPdf_(newDestination) && isNew) {
      this.updateDropdownDestinations_();
    }
  },

  /** @private */
  updateDropdownDestinations_: function() {
    this.displayedDestinations_ =
        /** @type {!Array<!RecentDestination>} */ (
            this.getSettingValue('recentDestinations'))
            .filter(d => {
              return !this.destinationIsDriveOrPdf_(d) &&
                  (d.origin !== DestinationOrigin.COOKIES ||
                   d.account === this.activeUser_);
            });
  },

  /**
   * @return {boolean} Whether the destinations dropdown should be disabled.
   * @private
   */
  shouldDisableDropdown_: function() {
    return this.state === State.FATAL_ERROR ||
        (this.destinationState === DestinationState.UPDATED && this.disabled &&
         this.state !== State.NOT_READY);
  },

  /** @private */
  computeShouldHideSpinner_: function() {
    return this.destinationState === DestinationState.ERROR ||
        this.destinationState === DestinationState.UPDATED ||
        (this.destinationState === DestinationState.SET && !!this.destination &&
         (!!this.destination.capabilities ||
          this.destination.id === Destination.GooglePromotedId.SAVE_AS_PDF));
  },

  /**
   * @return {string} The connection status text to display.
   * @private
   */
  computeStatusText_: function() {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return '';
    }

    return this.destination.shouldShowInvalidCertificateError ?
        this.i18n('noLongerSupportedFragment') :
        this.destination.connectionStatusText;
  },

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  computeHasPinSetting_: function() {
    return this.getSetting('pin').available;
  },
  // </if>

  /**
   * @param {!CustomEvent<string>} e Event containing the key of the recent
   *     destination that was selected, or "seeMore".
   * @private
   */
  onSelectedDestinationOptionChange_: function(e) {
    const value = e.detail;
    if (value === 'seeMore') {
      this.destinationStore_.startLoadAllDestinations();
      if (this.activeUser_) {
        this.invitationStore_.startLoadingInvitations(this.activeUser_);
      }
      this.$.destinationDialog.get().show();
      this.lastUser_ = this.activeUser_;
      this.isDialogOpen_ = true;
    } else {
      const success = this.destinationStore_.selectRecentDestinationByKey(
          value, this.displayedDestinations_);
      if (!success) {
        this.error = Error.INVALID_PRINTER;
      }
    }
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new active user
   *     account.
   * @private
   */
  onAccountChange_: function(e) {
    this.$.userManager.updateActiveUser(e.detail, true);
  },

  /** @private */
  onDialogClose_: function() {
    // Reset the select value if the user dismissed the dialog without
    // selecting a new destination.
    if (this.lastUser_ != this.activeUser_) {
      this.updateDropdownDestinations_();
    }
    this.updateDestinationSelect_();
    this.isDialogOpen_ = false;
  },

  /** @private */
  updateDestinationSelect_: function() {
    if (this.destinationState === DestinationState.ERROR && !this.destination) {
      return;
    }

    if (this.destinationState === DestinationState.INIT ||
        this.destinationState === DestinationState.SELECTED) {
      return;
    }

    const shouldFocus =
        this.destinationState !== DestinationState.SET && !this.firstLoad;
    beforeNextRender(this.$.destinationSelect, () => {
      this.$.destinationSelect.updateDestination();
      if (shouldFocus) {
        this.$.destinationSelect.focus();
      }
    });
  },
});
