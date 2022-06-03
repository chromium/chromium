// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import './shortcut_customization_shared_css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {ViewState} from './accelerator_view.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, AcceleratorType, ShortcutProviderInterface} from './shortcut_types.js';

/**
 * @fileoverview
 * 'accelerator-edit-view' is a wrapper component for one accelerator. It is
 * responsible for displaying the edit/remove buttons to an accelerator and also
 * displaying context or errors strings for an accelerator.
 */
export class AcceleratorEditViewElement extends PolymerElement {
  static get is() {
    return 'accelerator-edit-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!AcceleratorInfo} */
      acceleratorInfo: {
        type: Object,
        value: /** @type {!AcceleratorInfo} */ ({
          accelerator: /** @type {!AcceleratorKeys} */ ({
            modifiers: 0,
            key: 0,
            key_display: '',
          }),
          type: AcceleratorType.kDefault,
          state: AcceleratorState.kEnabled,
          locked: false,
        }),
      },

      isEditView: {
        type: Boolean,
        computed: 'showEditView_(viewState)',
        reflectToAttribute: true,
      },

      /** @private */
      isAddView_: {
        type: Boolean,
        computed: 'computeIsAddView_(viewState)',
        reflectToAttribute: true,
      },

      viewState: {
        type: Number,
        value: ViewState.VIEW,
        notify: true,
      },

      /** @protected */
      statusMessage: {
        type: String,
        value: '',
        observer: 'onStatusMessageChanged_',
      },

      hasError: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      action: {
        type: Number,
        value: 0,
      },

      /** @type {!AcceleratorSource} */
      source: {
        type: Number,
        value: 0,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!ShortcutProviderInterface} */
    this.shortcutProvider_ = getShortcutProvider();

    /** @private {!AcceleratorLookupManager} */
    this.lookupManager_ = AcceleratorLookupManager.getInstance();
  }

  /** @protected */
  onStatusMessageChanged_() {
    if (this.statusMessage === '') {
      // TODO(jimmyxgong): i18n this string.
      this.statusMessage =
          'Press 1-4 modifiers and 1 other key on your keyboard';
    }
  }

  /** @protected */
  onEditButtonClicked_() {
    this.viewState = ViewState.EDIT;
  }

  /** @protected */
  onDeleteButtonClicked_() {
    this.shortcutProvider_
        .removeAccelerator(
            this.source, this.action, this.acceleratorInfo.accelerator)
        .then((result) => {
          if (result === AcceleratorConfigResult.kSuccess) {
            this.lookupManager_.removeAccelerator(
                this.source, this.action, this.acceleratorInfo.accelerator);

            this.dispatchEvent(new CustomEvent('request-update-accelerator', {
              bubbles: true,
              composed: true,
              detail: {source: this.source, action: this.action}
            }));
          }
        });
  }

  /** @protected  */
  onCancelButtonClicked_() {
    this.statusMessage = '';
    this.viewState = ViewState.VIEW;
  }

  /**
   * @return {boolean}
   * @protected
   */
  showEditView_() {
    return this.viewState !== ViewState.VIEW;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeIsAddView_() {
    return this.viewState === ViewState.ADD;
  }
}

customElements.define(AcceleratorEditViewElement.is,
                      AcceleratorEditViewElement);
