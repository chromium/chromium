// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_dialog.js';
import './shortcut_input.js';
import './shortcuts_page.js';
import './shortcut_customization_fonts.css.js';
import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {getTemplate} from './shortcut_customization_app.html.js';
import {AcceleratorConfig, AcceleratorInfo, AcceleratorSource, AcceleratorState, AcceleratorType, LayoutInfoList, ShortcutProviderInterface} from './shortcut_types.js';

/**
 * @fileoverview
 * 'shortcut-customization-app' is the main landing page for the shortcut
 * customization app.
 */
export class ShortcutCustomizationAppElement extends PolymerElement {
  static get is() {
    return 'shortcut-customization-app';
  }

  static get properties() {
    return {
      /** @private */
      dialogShortcutTitle_: {
        type: String,
        value: '',
      },

      /**
       * @type {!Array<!AcceleratorInfo>}
       * @private
       */
      dialogAccelerators_: {
        type: Array,
        value: () => {},
      },

      /** @private */
      dialogAction_: {
        type: Number,
        value: 0,
      },

      /** @private {!AcceleratorSource} */
      dialogSource_: {
        type: Number,
        value: 0,
      },

      /** @private */
      showEditDialog_: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      showRestoreAllDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!ShortcutProviderInterface} */
    this.shortcutProvider_ = getShortcutProvider();

    /** @private {!AcceleratorLookupManager} */
    this.acceleratorLookupManager_ = AcceleratorLookupManager.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addNavigationSelectors_();
    this.fetchAccelerators_();

    window.addEventListener('show-edit-dialog',
        (e) => this.showDialog_(e.detail));
    window.addEventListener('edit-dialog-closed', () => this.onDialogClosed_());
    window.addEventListener(
        'request-update-accelerator',
        (e) => this.onRequestUpdateAccelerators_(e.detail));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('show-edit-dialog',
        (e) => this.showDialog_(e.detail));
    window.removeEventListener('edit-dialog-closed',
        () => this.onDialogClosed_());
  }

  /** @private */
  fetchAccelerators_() {
    // Kickoff fetching accelerators by first fetching the accelerator configs.
    this.shortcutProvider_.getAllAcceleratorConfig().then(
        (/** @type {!AcceleratorConfig}*/ result) =>
            this.onAcceleratorConfigFetched_(result));
  }

  /**
   * @param {!AcceleratorConfig} config
   * @private
   */
  onAcceleratorConfigFetched_(config) {
    this.acceleratorLookupManager_.setAcceleratorLookup(config);
    // After fetching the config infos, fetch the layout infos next.
    this.shortcutProvider_.getLayoutInfo().then(
        (/** @type {!LayoutInfoList} */ result) =>
            this.onLayoutInfosFetched_(result));
  }

  /**
   * @param {!LayoutInfoList} layoutInfos
   * @private
   */
  onLayoutInfosFetched_(layoutInfos) {
    this.acceleratorLookupManager_.setAcceleratorLayoutLookup(layoutInfos);
    // Notify pages to update their accelerators.
    this.$.navigationPanel.notifyEvent('updateAccelerators');
  }

  /** @private */
  addNavigationSelectors_() {
    const pages = [
      this.$.navigationPanel.createSelectorItem(
          'Chrome OS', 'shortcuts-page',
          'navigation-selector:laptop-chromebook', 'chromeos-page-id',
          {category: /**ChromeOS*/ 0}),
      this.$.navigationPanel.createSelectorItem(
          'Browser', 'shortcuts-page', 'navigation-selector:laptop-chromebook',
          'browser-page-id', {category: /**Browser*/ 1}),
      this.$.navigationPanel.createSelectorItem(
          'Android', 'shortcuts-page', 'navigation-selector:laptop-chromebook',
          'android-page-id', {category: /**Android*/ 2}),
      this.$.navigationPanel.createSelectorItem(
          'Accessibility', 'shortcuts-page',
          'navigation-selector:laptop-chromebook', 'a11y-page-id',
          {category: /**Accessbility*/ 3}),

    ];
    this.$.navigationPanel.addSelectors(pages);
  }

  /**
   * @param {!{description: string, accelerators: !Array<!AcceleratorInfo>,
   *           action: number, source: !AcceleratorSource}} e
   * @private
   */
  showDialog_(e) {
    this.dialogShortcutTitle_ = e.description;
    this.dialogAccelerators_ =
        /** @type {!Array<!AcceleratorInfo>}*/(e.accelerators);
    this.dialogAction_ = e.action;
    this.dialogSource_ = e.source;
    this.showEditDialog_ = true;
  }

  /** @private */
  onDialogClosed_() {
    this.showEditDialog_ = false;
    this.dialogShortcutTitle_ = '';
    this.dialogAccelerators_ = [];
  }

  /**
   * @param {!Object<number, number>} detail
   * @private
   */
  onRequestUpdateAccelerators_(detail) {
    this.$.navigationPanel.notifyEvent('updateSubsections');
    const updatedAccels =
        this.acceleratorLookupManager_
            .getAccelerators(detail.source, detail.action)
            .filter((accel) => {
              // Hide accelerators that are default and disabled.
              return !(accel.type === AcceleratorType.kDefault &&
                  accel.state === AcceleratorState.kDisabledByUser);
            });
    this.shadowRoot.querySelector('#editDialog')
        .updateDialogAccelerators(updatedAccels);
  }

  /** @protected */
  onRestoreAllDefaultClicked_() {
    this.showRestoreAllDialog_ = true;
  }

  /** @protected */
  onCancelRestoreButtonClicked_() {
    this.closeRestoreAllDialog_();
  }

  /** @protected */
  onConfirmRestoreButtonClicked_() {
    // TODO(jimmyxgong): Implement this function.
  }

  /** @protected */
  closeRestoreAllDialog_() {
    this.showRestoreAllDialog_ = false;
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    ShortcutCustomizationAppElement.is, ShortcutCustomizationAppElement);
