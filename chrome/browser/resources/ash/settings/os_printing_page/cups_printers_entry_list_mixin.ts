// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for observing CupsPrintersEntryManager events.
 * Use this behavior if you want to receive a dynamically updated list of both
 * saved and nearby printers.
 */

import {ListPropertyUpdateMixin, ListPropertyUpdateMixinInterface} from 'chrome://resources/ash/common/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Constructor} from '../common/types.js';

import {findDifference} from './cups_printer_dialog_util.js';
import {PrinterListEntry} from './cups_printer_types.js';
import {CupsPrintersEntryManager} from './cups_printers_entry_manager.js';

export interface CupsPrintersEntryListMixinInterface extends
    ListPropertyUpdateMixinInterface {
  enterprisePrinters: PrinterListEntry[];
  nearbyPrinters: PrinterListEntry[];
  savedPrinters: PrinterListEntry[];
  onSavedPrintersAdded(addedPrinters: PrinterListEntry[]): void;
  onSavedPrintersRemoved(removedPrinters: PrinterListEntry[]): void;
}

export const CupsPrintersEntryListMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<CupsPrintersEntryListMixinInterface> => {
      const superclassBase = ListPropertyUpdateMixin(superClass);

      class CupsPrintersEntryListMixinInternal extends superclassBase implements
          CupsPrintersEntryListMixinInterface {
        static get properties() {
          return {
            entryManager_: Object,

            savedPrinters: {
              type: Array,
              value: () => [],
            },

            nearbyPrinters: {
              type: Array,
              value: () => [],
            },

            enterprisePrinters: {
              type: Array,
              value: () => [],
            },
          };
        }

        enterprisePrinters: PrinterListEntry[];
        nearbyPrinters: PrinterListEntry[];
        savedPrinters: PrinterListEntry[];
        private entryManager_: CupsPrintersEntryManager;

        constructor() {
          super();

          this.entryManager_ = CupsPrintersEntryManager.getInstance();
        }

        override connectedCallback(): void {
          this.entryManager_.addOnSavedPrintersChangedListener(
              this.onSavedPrintersChanged_.bind(this));
          this.entryManager_.addOnNearbyPrintersChangedListener(
              this.onNearbyPrintersChanged_.bind(this));
          this.entryManager_.addOnEnterprisePrintersChangedListener(
              this.onEnterprisePrintersChanged_.bind(this));

          // Initialize saved and nearby printers list.
          this.onSavedPrintersChanged_(
              this.entryManager_.savedPrinters, [] /* printerAdded */,
              [] /* printerRemoved */);
          this.onNearbyPrintersChanged_(this.entryManager_.nearbyPrinters);
          this.onEnterprisePrintersChanged_(
              this.entryManager_.enterprisePrinters);
        }

        override disconnectedCallback(): void {
          this.entryManager_.removeOnSavedPrintersChangedListener(
              this.onSavedPrintersChanged_.bind(this));
          this.entryManager_.removeOnNearbyPrintersChangedListener(
              this.onNearbyPrintersChanged_.bind(this));
          this.entryManager_.removeOnEnterprisePrintersChangedListener(
              this.onEnterprisePrintersChanged_.bind(this));
        }

        /**
         * Non-empty params indicate the applicable change to be notified.
         */
        private onSavedPrintersChanged_(
            savedPrinters: PrinterListEntry[],
            addedPrinters: PrinterListEntry[],
            removedPrinters: PrinterListEntry[]): void {
          this.updateList(
              'savedPrinters', printer => printer.printerInfo.printerId,
              savedPrinters);

          assert(!(addedPrinters.length && removedPrinters.length));

          if (addedPrinters.length) {
            this.onSavedPrintersAdded(addedPrinters);
          } else if (removedPrinters.length) {
            this.onSavedPrintersRemoved(removedPrinters);
          }
        }

        private onNearbyPrintersChanged_(printerList: PrinterListEntry[]):
            void {
          // |printerList| consists of automatic and discovered printers that
          // have not been saved and are available. Add all unsaved print server
          // printers to |printerList|.
          this.entryManager_.printServerPrinters = findDifference(
              this.entryManager_.printServerPrinters, this.savedPrinters);
          printerList =
              printerList.concat(this.entryManager_.printServerPrinters);

          this.updateList(
              'nearbyPrinters', printer => printer.printerInfo.printerId,
              printerList);
        }

        private onEnterprisePrintersChanged_(enterprisePrinters:
                                                 PrinterListEntry[]): void {
          this.updateList(
              'enterprisePrinters', printer => printer.printerInfo.printerId,
              enterprisePrinters);
        }

        // Override in the custom element implementation
        onSavedPrintersAdded(_addedPrinters: PrinterListEntry[]): void {}

        // Override in the custom element implementation
        onSavedPrintersRemoved(_removedPrinters: PrinterListEntry[]): void {}
      }

      return CupsPrintersEntryListMixinInternal;
    });
