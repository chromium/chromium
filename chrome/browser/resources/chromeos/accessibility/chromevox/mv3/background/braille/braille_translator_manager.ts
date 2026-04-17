// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of the current braille translators.
 */
import {JP_BRAILLE_TENJI_TABLE} from '/chromevox/mv3/ash/common/braille_table.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BrailleTable} from '../../common/braille/braille_table.js';
import {SettingsManager} from '../../common/settings_manager.js';
import {Output} from '../output/output.js';

import {BrailleTranslator} from './braille_translator.js';
import {ExpandingBrailleTranslator} from './expanding_braille_translator.js';
import {LibLouis} from './liblouis.js';
import {TenjiTranslator} from './tenji_translator.js';

export class BrailleTranslatorManager {
  private liblouis_: LibLouis;
  private changeListeners_: VoidFunction[] = [];
  private tables_: BrailleTable.Table[] = [];
  private expandingTranslator_: ExpandingBrailleTranslator|null = null;
  private defaultTableId_: string|null = null;
  private defaultTranslator_: BrailleTranslator|null = null;
  private uncontractedTableId_: string|null = null;
  private uncontractedTranslator_: BrailleTranslator|null = null;

  static instance: BrailleTranslatorManager;

  constructor(liblouisForTest?: LibLouis) {
    this.liblouis_ =
        liblouisForTest ||
        new LibLouis(
            chrome.runtime.getURL(
                'chromevox/third_party/liblouis/liblouis_wrapper.js'),
            chrome.runtime.getURL('chromevox/mv3/background/braille/tables'),
            () => this.loadLiblouis_());
  }

  static init(): void {
    if (BrailleTranslatorManager.instance) {
      throw new Error('\nCannot create two BrailleTranslatorManagers');
    }
    BrailleTranslatorManager.instance = new BrailleTranslatorManager();

    SettingsManager.addListenerForKey(
        'brailleTable',
        (brailleTable: string) =>
            BrailleTranslatorManager.instance.refresh(brailleTable));
  }

  static backTranslate(cells: ArrayBuffer): Promise<string|null> {
    return new Promise(resolve => {
      const translator =
          BrailleTranslatorManager.instance.getDefaultTranslator();
      if (!translator) {
        console.error('Braille translator is null, cannot call backTranslate');
        resolve('');
        return;
      }
      translator.backTranslate(cells, resolve);
    });
  }

  /**
   * Adds a listener to be called whenever there is a change in the
   * translator(s) returned by other methods of this instance.
   */
  addChangeListener(listener: VoidFunction): void {
    this.changeListeners_.push(listener);
  }

  private getCurrentLocale_(): string[] {
    return chrome.i18n.getMessage('@@ui_locale').split(/[_-]/);
  }

  private useTenjiTranslator_(tableId: string): boolean {
    // Tenji is only used for Japanese. So use it when no table is specified and
    // the current locale is Japanese, or the specified table is the Japanese
    // tenji table.
    return (!tableId && this.getCurrentLocale_()[0] === 'ja') ||
        (tableId === JP_BRAILLE_TENJI_TABLE.id);
  }

  /**
   * Refreshes the braille translator(s) used for input and output.  This
   * should be called when something has changed (such as a preference) to
   * make sure that the correct translator is used.
   * @param brailleTable The table for this translator to use.
   * @param brailleTable8 Optionally specify an uncontracted table.
   * @param finishCallback Called when the refresh finishes.
   */
  async refresh(
      brailleTable: string, brailleTable8?: string,
      finishCallback?: VoidFunction): Promise<void> {
    finishCallback = finishCallback ?? (() => {});
    if (brailleTable && brailleTable === this.defaultTableId_) {
      finishCallback();
      return;
    }

    // First check if the Tenji translator should be used, since it is a special
    // case that does not rely on liblouis tables.
    if (!(this.defaultTranslator_ instanceof TenjiTranslator) &&
        this.useTenjiTranslator_(brailleTable)) {
      const tenjiTranslator = new TenjiTranslator();
      try {
        await tenjiTranslator.init();
        this.defaultTableId_ = JP_BRAILLE_TENJI_TABLE.id;
        this.defaultTranslator_ = tenjiTranslator;
        this.expandingTranslator_ =
            new ExpandingBrailleTranslator(tenjiTranslator, null);
        this.uncontractedTableId_ = null;
        this.uncontractedTranslator_ = null;
        this.changeListeners_.forEach(listener => listener());
        finishCallback();
        return;
      } catch (error) {
        console.error('Failed to initialize TenjiTranslator', error);
        // Fall back on liblouis translator with its ja-kantenji table if Tenji
        // translator init fails.
        brailleTable = 'ja-kantenji';
      }
    }

    // Initialize a liblouis translator.
    const tables = this.tables_;
    if (tables.length === 0) {
      finishCallback();
      return;
    }
    // Look for the table requested.
    let table = BrailleTable.forId(tables, brailleTable);
    if (!table) {
      // Match table against current locale.
      const currentLocale = this.getCurrentLocale_();
      const major = currentLocale[0];
      const minor = currentLocale[1];
      const firstPass =
          tables.filter(table => table.locale.split(/[_-]/)[0] === major);
      if (firstPass.length > 0) {
        table = firstPass[0];
        if (minor) {
          const secondPass = firstPass.filter(
              table => table.locale.split(/[_-]/)[1] === minor);
          if (secondPass.length > 0) {
            table = secondPass[0];
          }
        }
      }
    }
    if (!table) {
      table = BrailleTable.forId(tables, 'en-nabcc')!;
    }

    // If the user explicitly set an 8 dot table, use that when looking
    // for an uncontracted table.  Otherwise, use the current table and let
    // getUncontracted find an appropriate corresponding table.
    const table8Dot =
        brailleTable8 ? BrailleTable.forId(tables, brailleTable8) : null;
    const uncontractedTable =
        BrailleTable.getUncontracted(tables, table8Dot ?? table);
    const newDefaultTableId = table.id;
    const newUncontractedTableId =
        table.id === uncontractedTable.id ? null : uncontractedTable.id;
    if (newDefaultTableId === this.defaultTableId_ &&
        newUncontractedTableId === this.uncontractedTableId_) {
      finishCallback();
      return;
    }

    const finishRefresh =
        (defaultTranslator: BrailleTranslator|null,
         uncontractedTranslator: BrailleTranslator|null): void => {
          this.defaultTableId_ = newDefaultTableId;
          this.uncontractedTableId_ = newUncontractedTableId;
          // TODO(crbug.com/314203187): Not null asserted, check that this is
          // correct.
          this.expandingTranslator_ = defaultTranslator ?
              new ExpandingBrailleTranslator(
                  defaultTranslator!, uncontractedTranslator) :
              null;
          this.defaultTranslator_ = defaultTranslator;
          this.uncontractedTranslator_ = uncontractedTranslator;
          this.changeListeners_.forEach(listener => listener());
          finishCallback();
        };

    const translator = await this.liblouis_.getTranslator(table.fileNames);
    if (!newUncontractedTableId) {
      finishRefresh(translator, null);
    } else {
      const uncontractedTranslator =
          await this.liblouis_.getTranslator(uncontractedTable.fileNames);
      finishRefresh(translator, uncontractedTranslator);
    }
  }

  /**
   * @return The current expanding braille translator, or null if none is
   * available.
   */
  getExpandingTranslator(): ExpandingBrailleTranslator|null {
    return this.expandingTranslator_;
  }

  /**
   * @return The current braille translator to use by default, or null if none
   * is available.
   */
  getDefaultTranslator(): BrailleTranslator|null {
    return this.defaultTranslator_;
  }

  /**
   * @return The current uncontracted braille translator, or null if it is the
   * same as the default translator.
   */
  getUncontractedTranslator(): BrailleTranslator|null {
    return this.uncontractedTranslator_;
  }

  /** Toggles the braille table type. */
  toggleBrailleTable(): void {
    let brailleTableType = SettingsManager.getString('brailleTableType');
    let output = '';
    if (brailleTableType === 'brailleTable6') {
      brailleTableType = 'brailleTable8';

      // This label reads "switch to 8 dot braille".
      output = '@OPTIONS_BRAILLE_TABLE_TYPE_6';
    } else {
      brailleTableType = 'brailleTable6';

      // This label reads "switch to 6 dot braille".
      output = '@OPTIONS_BRAILLE_TABLE_TYPE_8';
    }

    const brailleTable = SettingsManager.getString(brailleTableType);
    SettingsManager.set('brailleTable', brailleTable);
    SettingsManager.set('brailleTableType', brailleTableType);
    this.refresh(brailleTable);
    new Output().format(output).go();
  }

  /**
   * Asynchronously fetches the list of braille tables and refreshes the
   * translators when done.
   * Resolves when tables are loaded.
   */
  private async fetchTables_(): Promise<void> {
    return new Promise((r: VoidFunction) => {
      BrailleTable.getAll(tables => {
        this.tables_ = tables;

        // Initial refresh; set options from user preferences.
        this.refresh(SettingsManager.getString('brailleTable'), undefined, r);
      });
    });
  }

  /**
   * Loads the liblouis instance by attaching it to the document.
   */
  private loadLiblouis_(): void {
    this.fetchTables_();
  }

  getLibLouisForTest(): LibLouis {
    return this.liblouis_;
  }

  /**
   * @return The currently loaded braille tables, or an empty array if they are
   * not yet loaded.
   */
  getTablesForTest(): BrailleTable.Table[] {
    return this.tables_;
  }

  /** Loads liblouis tables and returns a promise resolved when loaded. */
  async loadTablesForTest(): Promise<void> {
    await this.fetchTables_();
  }
}

TestImportManager.exportForTesting(BrailleTranslatorManager);
