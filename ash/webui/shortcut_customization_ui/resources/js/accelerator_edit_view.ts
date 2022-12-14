// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../common/icons.html.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_edit_view.html.js';
import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {ViewState} from './accelerator_view.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorSource, AcceleratorState, AcceleratorType, DefaultAcceleratorInfo, ShortcutProviderInterface} from './shortcut_types.js';
import {getAccelerator} from './shortcut_utils.js';

export type RequestUpdateAcceleratorEvent =
    CustomEvent<{action: number, source: AcceleratorSource}>;

declare global {
  interface HTMLElementEventMap {
    'request-update-accelerator': RequestUpdateAcceleratorEvent;
  }
}

const accelerator: Accelerator = {
  modifiers: 0,
  keyCode: 0,
};

const defaultAcceleratorInfoState: DefaultAcceleratorInfo = {
  locked: false,
  state: AcceleratorState.kEnabled,
  type: AcceleratorType.kDefault,
  layoutProperties: {
    defaultAccelerator: {
      accelerator,
      keyDisplay: '',
    },
  },
};

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
    return getTemplate();
  }

  static get properties() {
    return {
      acceleratorInfo: {
        type: Object,
        value: defaultAcceleratorInfoState,
      },

      isEditView: {
        type: Boolean,
        computed: 'showEditView_(viewState)',
        reflectToAttribute: true,
      },

      viewState: {
        type: Number,
        value: ViewState.VIEW,
        notify: true,
      },

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

      source: {
        type: Number,
        value: 0,
      },
    };
  }

  acceleratorInfo: DefaultAcceleratorInfo;
  isEditView: boolean;
  viewState: number;
  hasError: boolean;
  action: number;
  source: AcceleratorSource;
  protected statusMessage: string;
  private shortcutProvider_: ShortcutProviderInterface;
  private lookupManager_: AcceleratorLookupManager;

  constructor() {
    super();

    this.shortcutProvider_ = getShortcutProvider();

    this.lookupManager_ = AcceleratorLookupManager.getInstance();
  }

  protected onStatusMessageChanged_() {
    if (this.statusMessage === '') {
      // TODO(jimmyxgong): i18n this string.
      this.statusMessage =
          'Press 1-4 modifiers and 1 other key on your keyboard';
    }
  }

  protected onEditButtonClicked_() {
    this.viewState = ViewState.EDIT;
  }

  protected onDeleteButtonClicked_() {
    this.shortcutProvider_
        .removeAccelerator(
            this.source, this.action, getAccelerator(this.acceleratorInfo))
        .then((result: AcceleratorConfigResult) => {
          if (result === AcceleratorConfigResult.SUCCESS) {
            this.lookupManager_.removeAccelerator(
                this.source, this.action, getAccelerator(this.acceleratorInfo));

            this.dispatchEvent(new CustomEvent('request-update-accelerator', {
              bubbles: true,
              composed: true,
              detail: {source: this.source, action: this.action},
            }));
          }
        });
  }

  protected onCancelButtonClicked_() {
    this.statusMessage = '';
    this.viewState = ViewState.VIEW;
  }

  protected showEditView_(): boolean {
    return this.viewState !== ViewState.VIEW;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-edit-view': AcceleratorEditViewElement;
  }
}

customElements.define(
    AcceleratorEditViewElement.is, AcceleratorEditViewElement);
