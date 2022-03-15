// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
// <if expr="not chromeos and not lacros">
import './destination_dialog.js';
// </if>
// <if expr="chromeos_ash or chromeos_lacros">
import './destination_dialog_cros.js';
// </if>
// <if expr="not chromeos and not lacros">
import './destination_select.js';
// </if>
// <if expr="chromeos_ash or chromeos_lacros">
import './destination_select_cros.js';
// </if>
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import './throbber_css.js';
import './settings_section.js';
import '../data/user_manager.js';
import '../strings.m.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {beforeNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CloudPrintInterfaceImpl} from '../cloud_print_interface_impl.js';
import {createRecentDestinationKey, Destination, DestinationOrigin, GooglePromotedDestinationId, makeRecentDestination, RecentDestination} from '../data/destination.js';
// <if expr="chromeos_ash or chromeos_lacros">
import {SAVE_TO_DRIVE_CROS_DESTINATION_KEY} from '../data/destination.js';
// </if>
import {getPrinterTypeForDestination, PrinterType} from '../data/destination_match.js';
import {DestinationErrorType, DestinationStore, DestinationStoreEventType} from '../data/destination_store.js';
import {Error, State} from '../data/state.js';

// <if expr="not chromeos and not lacros">
import {PrintPreviewDestinationDialogElement} from './destination_dialog.js';
// </if>
// <if expr="chromeos_ash or chromeos_lacros">
import {PrintPreviewDestinationDialogCrosElement} from './destination_dialog_cros.js';
// </if>
// <if expr="not chromeos and not lacros">
import {PrintPreviewDestinationSelectElement} from './destination_select.js';
// </if>
// <if expr="chromeos_ash or chromeos_lacros">
import {PrintPreviewDestinationSelectCrosElement} from './destination_select_cros.js';
// </if>
import {getTemplate} from './destination_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

export enum DestinationState {
  INIT = 0,
  SELECTED = 1,
  SET = 2,
  UPDATED = 3,
  ERROR = 4,
}

/** Number of recent destinations to save. */
// <if expr="not chromeos and not lacros">
export const NUM_PERSISTED_DESTINATIONS: number = 5;
// </if>
// <if expr="chromeos_ash or chromeos_lacros">
export const NUM_PERSISTED_DESTINATIONS: number = 10;
// </if>

/**
 * Number of unpinned recent destinations to display.
 * Pinned destinations include "Save as PDF" and "Save to Google Drive".
 */
const NUM_UNPINNED_DESTINATIONS: number = 3;

export interface PrintPreviewDestinationSettingsElement {
  $: {
    // <if expr="not chromeos and not lacros">
    destinationDialog:
        CrLazyRenderElement<PrintPreviewDestinationDialogElement>,
    destinationSelect: PrintPreviewDestinationSelectElement,
    // </if>
    // <if expr="chromeos_ash or chromeos_lacros">
    destinationDialog:
        CrLazyRenderElement<PrintPreviewDestinationDialogCrosElement>,
    destinationSelect: PrintPreviewDestinationSelectCrosElement,
    // </if>
  };
}

const PrintPreviewDestinationSettingsElementBase =
    I18nMixin(WebUIListenerMixin(SettingsMixin(PolymerElement)));

export class PrintPreviewDestinationSettingsElement extends
    PrintPreviewDestinationSettingsElementBase {
  static get is() {
    return 'print-preview-destination-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dark: Boolean,

      destination: {
        type: Object,
        notify: true,
        value: null,
      },

      destinationState: {
        type: Number,
        notify: true,
        value: DestinationState.INIT,
        observer: 'updateDestinationSelect_',
      },

      disabled: Boolean,

      error: {
        type: Number,
        notify: true,
        observer: 'onErrorChanged_',
      },

      firstLoad: Boolean,

      state: Number,

      activeUser_: {
        type: String,
        observer: 'onActiveUserChanged_',
      },

      cloudPrintDisabled_: {
        type: Boolean,
        value: true,
      },

      destinationStore_: {
        type: Object,
        value: null,
      },

      displayedDestinations_: Array,

      // <if expr="chromeos_ash or chromeos_lacros">
      driveDestinationKey_: {
        type: String,
        value: '',
      },

      hasPinSetting_: {
        type: Boolean,
        computed: 'computeHasPinSetting_(settings.pin.available)',
        reflectToAttribute: true,
      },
      // </if>

      isDialogOpen_: {
        type: Boolean,
        value: false,
      },

      noDestinations_: {
        type: Boolean,
        value: false,
      },

      pdfPrinterDisabled_: Boolean,

      loaded_: {
        type: Boolean,
        computed: 'computeLoaded_(destinationState, destination)',
      },

      users_: Array,
    };
  }

  dark: boolean;
  destination: Destination;
  destinationState: DestinationState;
  disabled: boolean;
  error: Error;
  firstLoad: boolean;
  state: State;
  private activeUser_: string;
  private cloudPrintDisabled_: boolean;
  private destinationStore_: DestinationStore|null;
  private displayedDestinations_: Destination[];

  // <if expr="chromeos_ash or chromeos_lacros">
  private driveDestinationKey_: string;
  private hasPinSetting_: boolean;
  // </if>

  private isDialogOpen_: boolean;
  private noDestinations_: boolean;
  private pdfPrinterDisabled_: boolean;
  private loaded_: boolean;
  private users_: string[];

  private lastUser_: string = '';
  private tracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    this.destinationStore_ =
        new DestinationStore(this.addWebUIListener.bind(this));
    this.tracker_.add(
        this.destinationStore_, DestinationStoreEventType.DESTINATION_SELECT,
        this.onDestinationSelect_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        DestinationStoreEventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        this.onDestinationCapabilitiesReady_.bind(this));
    this.tracker_.add(
        this.destinationStore_, DestinationStoreEventType.ERROR,
        this.onDestinationError_.bind(this));
    // Need to update the recent list when the destination store inserts
    // destinations, in case any recent destinations have been added to the
    // store. At startup, recent destinations can be in the sticky settings,
    // but they should not be displayed in the dropdown until they have been
    // fetched by the DestinationStore, to ensure that they still exist.
    this.tracker_.add(
        this.destinationStore_, DestinationStoreEventType.DESTINATIONS_INSERTED,
        this.updateDropdownDestinations_.bind(this));

    // <if expr="chromeos_ash or chromeos_lacros">
    this.tracker_.add(
        this.destinationStore_,
        DestinationStoreEventType.DESTINATION_EULA_READY,
        this.updateDestinationEulaUrl_.bind(this));
    // </if>
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.destinationStore_!.resetTracker();
    this.tracker_.removeAll();
  }

  private onActiveUserChanged_() {
    this.updateDropdownDestinations_();

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
      this.destinationStore_!.selectDestination(recent);
      return;
    }
    this.destinationStore_!.selectDefaultDestination();
  }

  /**
   * @param defaultPrinter The system default printer ID.
   * @param pdfPrinterDisabled Whether the PDF printer is disabled.
   * @param isDriveMounted Whether Google Drive is mounted. Only used
        on Chrome OS.
   * @param serializedDefaultDestinationRulesStr String with rules
   *     for selecting a default destination.
   */
  init(
      defaultPrinter: string, pdfPrinterDisabled: boolean,
      isDriveMounted: boolean,
      serializedDefaultDestinationRulesStr: string|null) {
    const cloudPrintInterface = CloudPrintInterfaceImpl.getInstance();
    this.pdfPrinterDisabled_ = pdfPrinterDisabled;
    let recentDestinations =
        this.getSettingValue('recentDestinations') as RecentDestination[];
    // <if expr="chromeos_ash or chromeos_lacros">
    this.driveDestinationKey_ =
        isDriveMounted ? SAVE_TO_DRIVE_CROS_DESTINATION_KEY : '';
    // </if>

    if (cloudPrintInterface.isConfigured()) {
      this.cloudPrintDisabled_ = false;
      this.destinationStore_!.setCloudPrintInterface(cloudPrintInterface);
      beforeNextRender(this, () => {
        this.shadowRoot!.querySelector(
                            'print-preview-user-manager')!.initUserAccounts();
        recentDestinations = recentDestinations.slice(
            0, this.getRecentDestinationsDisplayCount_(recentDestinations));
        this.destinationStore_!.init(
            this.pdfPrinterDisabled_, isDriveMounted, defaultPrinter,
            serializedDefaultDestinationRulesStr, recentDestinations);
      });
      return;
    }

    recentDestinations = recentDestinations.slice(
        0, this.getRecentDestinationsDisplayCount_(recentDestinations));
    this.destinationStore_!.init(
        this.pdfPrinterDisabled_, isDriveMounted, defaultPrinter,
        serializedDefaultDestinationRulesStr, recentDestinations);
  }

  /**
   * @param recentDestinations recent destinations.
   * @return Number of recent destinations to display.
   */
  private getRecentDestinationsDisplayCount_(recentDestinations:
                                                 RecentDestination[]): number {
    let numDestinationsToDisplay = NUM_UNPINNED_DESTINATIONS;
    for (let i = 0; i < recentDestinations.length; i++) {
      // Once all NUM_UNPINNED_DESTINATIONS unpinned destinations have been
      // found plus an extra unpinned destination, return the total number of
      // destinations found excluding the last extra unpinned destination.
      //
      // The extra unpinned destination ensures that pinned destinations
      // located directly after the last unpinned destination are included
      // in the display count.
      if (i > numDestinationsToDisplay) {
        return numDestinationsToDisplay;
      }
      // If a destination is pinned, increment numDestinationsToDisplay.
      if (this.destinationIsDriveOrPdf_(recentDestinations[i])) {
        numDestinationsToDisplay++;
      }
    }
    return Math.min(recentDestinations.length, numDestinationsToDisplay);
  }

  private onDestinationSelect_() {
    // If the user selected a destination in the dialog after changing the
    // active user, do the UI updates that were previously deferred.
    if (this.isDialogOpen_ && this.lastUser_ !== this.activeUser_) {
      this.updateDropdownDestinations_();
    }

    if (this.state === State.FATAL_ERROR) {
      // Don't let anything reset if there is a fatal error.
      return;
    }

    const destination = this.destinationStore_!.selectedDestination!;
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
  }

  private onDestinationCapabilitiesReady_() {
    this.notifyPath('destination.capabilities');
    this.updateRecentDestinations_();
    if (this.destinationState === DestinationState.SET) {
      this.destinationState = DestinationState.UPDATED;
    }
  }

  private onDestinationError_(e: CustomEvent<DestinationErrorType>) {
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
  }

  private onErrorChanged_() {
    if (this.error === Error.INVALID_PRINTER ||
        this.error === Error.UNSUPPORTED_PRINTER ||
        this.error === Error.NO_DESTINATIONS) {
      this.destinationState = DestinationState.ERROR;
    }
  }

  /**
   * @return Whether the destination is Save as PDF or Save to Drive.
   */
  private destinationIsDriveOrPdf_(destination: RecentDestination): boolean {
    // <if expr="chromeos_ash or chromeos_lacros">
    if (destination.id === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
      return true;
    }
    // </if>

    return destination.id === GooglePromotedDestinationId.SAVE_AS_PDF;
  }

  private updateRecentDestinations_() {
    if (!this.destination) {
      return;
    }

    // Determine if this destination is already in the recent destinations,
    // where in the array it is located, and whether or not it is visible.
    const newDestination = makeRecentDestination(this.destination);
    const recentDestinations =
        this.getSettingValue('recentDestinations') as RecentDestination[];
    let indexFound = -1;
    // Note: isVisible should be only be used if the destination is unpinned.
    // Although pinned destinations are always visible, isVisible may not
    // necessarily be set to true in this case.
    let isVisible = false;
    let numUnpinnedChecked = 0;
    for (let index = 0; index < recentDestinations.length; index++) {
      const recent = recentDestinations[index];
      if (recent.id === newDestination.id &&
          recent.origin === newDestination.origin) {
        indexFound = index;
        // If we haven't seen the maximum unpinned destinations already, this
        // destination is visible in the dropdown.
        isVisible = numUnpinnedChecked < NUM_UNPINNED_DESTINATIONS;
        break;
      }
      if (!this.destinationIsDriveOrPdf_(recent)) {
        numUnpinnedChecked++;
      }
    }

    // No change
    if (indexFound === 0 &&
        recentDestinations[0].capabilities === newDestination.capabilities) {
      return;
    }
    const isNew = indexFound === -1;

    // Shift the array so that the nth most recent destination is located at
    // index n.
    if (isNew && recentDestinations.length === NUM_PERSISTED_DESTINATIONS) {
      indexFound = NUM_PERSISTED_DESTINATIONS - 1;
    }

    if (indexFound !== -1) {
      this.setSettingSplice('recentDestinations', indexFound, 1, null);
    }

    // Add the most recent destination
    this.setSettingSplice('recentDestinations', 0, 0, newDestination);

    // The dropdown needs to be updated if a new printer or one not currently
    // visible in the dropdown has been added.
    if (!this.destinationIsDriveOrPdf_(newDestination) &&
        (isNew || !isVisible)) {
      this.updateDropdownDestinations_();
    }
  }

  private updateDropdownDestinations_() {
    const recentDestinations =
        this.getSettingValue('recentDestinations') as RecentDestination[];
    const updatedDestinations: Destination[] = [];
    let numDestinationsChecked = 0;
    for (const recent of recentDestinations) {
      if (this.destinationIsDriveOrPdf_(recent)) {
        continue;
      }
      numDestinationsChecked++;
      const key = createRecentDestinationKey(recent);
      const destination = this.destinationStore_!.getDestinationByKey(key);
      if (destination &&
          (!destination.account || destination.account === this.activeUser_)) {
        updatedDestinations.push(destination);
      }
      if (numDestinationsChecked === NUM_UNPINNED_DESTINATIONS) {
        break;
      }
    }

    this.displayedDestinations_ = updatedDestinations;
  }

  /**
   * @return Whether the destinations dropdown should be disabled.
   */
  private shouldDisableDropdown_(): boolean {
    return this.state === State.FATAL_ERROR ||
        (this.destinationState === DestinationState.UPDATED && this.disabled &&
         this.state !== State.NOT_READY);
  }

  private computeLoaded_(): boolean {
    return this.destinationState === DestinationState.ERROR ||
        this.destinationState === DestinationState.UPDATED ||
        (this.destinationState === DestinationState.SET && !!this.destination &&
         (!!this.destination.capabilities ||
          getPrinterTypeForDestination(this.destination) ===
              PrinterType.PDF_PRINTER));
  }

  // <if expr="chromeos_ash or chromeos_lacros">
  private computeHasPinSetting_(): boolean {
    return this.getSetting('pin').available;
  }
  // </if>

  /**
   * @param e Event containing the key of the recent destination that was
   *     selected, or "seeMore".
   */
  private onSelectedDestinationOptionChange_(e: CustomEvent<string>) {
    const value = e.detail;
    if (value === 'seeMore') {
      this.destinationStore_!.startLoadAllDestinations();
      this.$.destinationDialog.get().show();
      this.lastUser_ = this.activeUser_;
      this.isDialogOpen_ = true;
    } else {
      this.destinationStore_!.selectDestinationByKey(value);
    }
  }

  /**
   * @param e Event containing the new active user account.
   */
  private onAccountChange_(e: CustomEvent<string>) {
    assert(!this.cloudPrintDisabled_);
    this.shadowRoot!.querySelector('print-preview-user-manager')!
        .updateActiveUser(e.detail);
  }

  private onDialogClose_() {
    if (this.lastUser_ !== this.activeUser_) {
      this.updateDropdownDestinations_();
    }

    // Reset the select value if the user dismissed the dialog without
    // selecting a new destination.
    this.updateDestinationSelect_();
    this.isDialogOpen_ = false;
  }

  private updateDestinationSelect_() {
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
  }

  getDestinationStoreForTest(): DestinationStore {
    assert(this.destinationStore_);
    return this.destinationStore_;
  }

  // <if expr="chromeos_ash or chromeos_lacros">
  /**
   * @param e Event containing the current destination's EULA URL.
   */
  private updateDestinationEulaUrl_(e: CustomEvent<string>) {
    if (!this.destination) {
      return;
    }

    this.destination.eulaUrl = e.detail;
    this.notifyPath('destination.eulaUrl');
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-settings':
        PrintPreviewDestinationSettingsElement;
  }
}

customElements.define(
    PrintPreviewDestinationSettingsElement.is,
    PrintPreviewDestinationSettingsElement);
