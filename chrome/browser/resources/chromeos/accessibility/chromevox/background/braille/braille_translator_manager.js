// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of the current braille translators.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BrailleTable} from '../../common/braille/braille_table.js';
import {SettingsManager} from '../../common/settings_manager.js';
import {Output} from '../output/output.js';

import {ExpandingBrailleTranslator} from './expanding_braille_translator.js';
import {LibLouis} from './liblouis.js';

export class BrailleTranslatorManager {
  /**
   * @param {LibLouis=} opt_liblouisForTest Liblouis instance to use
   *     for testing.
   */
  constructor(opt_liblouisForTest) {
    /** @private {!LibLouis} */
    this.liblouis_ =
        opt_liblouisForTest ||
        new LibLouis(
            chrome.extension.getURL(
                'chromevox/third_party/liblouis/liblouis_wrapper.js'),
            chrome.extension.getURL('chromevox/background/braille/tables'),
            () => this.loadLiblouis_());

    /** @private {!Array<function()>} */
    this.changeListeners_ = [];
    /** @private {!Array<BrailleTable.Table>} */
    this.tables_ = [];
    /** @private {?ExpandingBrailleTranslator} */
    this.expandingTranslator_ = null;
    /** @private {?LibLouis.Translator} */
    this.defaultTranslator_ = null;
    /** @private {?string} */
    this.defaultTableId_ = null;
    /** @private {?LibLouis.Translator} */
    this.uncontractedTranslator_ = null;
    /** @private {?string} */
    this.uncontractedTableId_ = null;
  }

  static init() {
    if (BrailleTranslatorManager.instance) {
      throw new Error('\nCannot create two BrailleTranslatorManagers');
    }
    BrailleTranslatorManager.instance = new BrailleTranslatorManager();

    SettingsManager.addListenerForKey(
        'brailleTable',
        brailleTable =>
            BrailleTranslatorManager.instance.refresh(brailleTable));
  }

  /**
   * @param {!ArrayBuffer} cells
   * @return {!Promise<?string>}
   */
  static backTranslate(cells) {
    return new Promise(resolve => {
      BrailleTranslatorManager.instance.getDefaultTranslator().backTranslate(
          cells, resolve);
    });
  }

  /**
   * Adds a listener to be called whenever there is a change in the
   * translator(s) returned by other methods of this instance.
   * @param {function()} listener The listener.
   */
  addChangeListener(listener) {
    this.changeListeners_.push(listener);
  }

  /**
   * Refreshes the braille translator(s) used for input and output.  This
   * should be called when something has changed (such as a preference) to
   * make sure that the correct translator is used.
   * @param {string} brailleTable The table for this translator to use.
   * @param {string=} opt_brailleTable8 Optionally specify an uncontracted
   * table.
   * @param {function()=} opt_finishCallback Called when the refresh finishes.
   */
  async refresh(brailleTable, opt_brailleTable8, opt_finishCallback) {
    const finishCallback = opt_finishCallback || (() => {});
    if (brailleTable && brailleTable === this.defaultTableId_) {
      finishCallback();
      return;
    }

    const tables = this.tables_;
    if (tables.length === 0) {
      finishCallback();
      return;
    }

    // Look for the table requested.
    let table = BrailleTable.forId(tables, brailleTable);
    if (!table) {
      // Match table against current locale.
      const currentLocale = chrome.i18n.getMessage('@@ui_locale').split(/[_-]/);
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
      table = BrailleTable.forId(tables, 'en-nabcc');
    }

    // If the user explicitly set an 8 dot table, use that when looking
    // for an uncontracted table.  Otherwise, use the current table and let
    // getUncontracted find an appropriate corresponding table.
    const table8Dot = opt_brailleTable8 ?
        BrailleTable.forId(tables, opt_brailleTable8) :
        null;
    const uncontractedTable =
        BrailleTable.getUncontracted(tables, table8Dot || table);
    const newDefaultTableId = table.id;
    const newUncontractedTableId =
        table.id === uncontractedTable.id ? null : uncontractedTable.id;
    if (newDefaultTableId === this.defaultTableId_ &&
        newUncontractedTableId === this.uncontractedTableId_) {
      finishCallback();
      return;
    }

    const finishRefresh = (defaultTranslator, uncontractedTranslator) => {
      this.defaultTableId_ = newDefaultTableId;
      this.uncontractedTableId_ = newUncontractedTableId;
      this.expandingTranslator_ = new ExpandingBrailleTranslator(
          defaultTranslator, uncontractedTranslator);
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
   * @return {ExpandingBrailleTranslator} The current expanding braille
   *     translator, or {@code null} if none is available.
   */
  getExpandingTranslator() {
    return this.expandingTranslator_;
  }

  /**
   * @return {LibLouis.Translator} The current braille translator to use
   *     by default, or {@code null} if none is available.
   */
  getDefaultTranslator() {
    return this.defaultTranslator_;
  }

  /**
   * @return {LibLouis.Translator} The current uncontracted braille
   *     translator, or {@code null} if it is the same as the default
   *     translator.
   */
  getUncontractedTranslator() {
    return this.uncontractedTranslator_;
  }

  /** Toggles the braille table type. */
  toggleBrailleTable() {
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
   * @return {!Promise} Resolves when tables are loaded.
   * @private
   */
  async fetchTables_() {
    return new Promise(r => {
      BrailleTable.getAll(tables => {
        this.tables_ = tables;

        // Initial refresh; set options from user preferences.
        this.refresh(SettingsManager.getString('brailleTable'), undefined, r);
      });
    });
  }

  /**
   * Loads the liblouis instance by attaching it to the document.
   * @private
   */
  loadLiblouis_() {
    this.fetchTables_();
  }

  /**
   * @return {!LibLouis} The liblouis instance used by this object.
   */
  getLibLouisForTest() {
    return this.liblouis_;
  }

  /**
   * @return {!Array<BrailleTable.Table>} The currently loaded braille
   *     tables, or an empty array if they are not yet loaded.
   */
  getTablesForTest() {
    return this.tables_;
  }

  /**
   * Loads liblouis tables and returns a promise resolved when loaded.
   * @return {!Promise}
   */
  async loadTablesForTest() {
    await this.fetchTables_();
  }
}

/** @type {BrailleTranslatorManager} */
BrailleTranslatorManager.instance;

TestImportManager.exportForTesting(BrailleTranslatorManager);
