// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../common/icons.html.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorResultData} from '../mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';

import {getTemplate} from './accelerator_edit_view.html.js';
import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {ViewState} from './accelerator_view.js';
import {FakeShortcutProvider} from './fake_shortcut_provider.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorSource, AcceleratorState, AcceleratorType, ShortcutProviderInterface, StandardAcceleratorInfo} from './shortcut_types.js';
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

const standardAcceleratorInfoState: StandardAcceleratorInfo = {
  locked: false,
  state: AcceleratorState.kEnabled,
  type: AcceleratorType.kDefault,
  layoutProperties: {
    standardAccelerator: {
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
const AcceleratorEditViewElementBase = I18nMixin(PolymerElement);

export class AcceleratorEditViewElement extends AcceleratorEditViewElementBase {
  static get is(): string {
    return 'accelerator-edit-view';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      acceleratorInfo: {
        type: Object,
        value: standardAcceleratorInfoState,
      },

      isEditView: {
        type: Boolean,
        computed: 'showEditView(viewState)',
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
        observer: AcceleratorEditViewElement.prototype.onStatusMessageChanged,
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

  acceleratorInfo: StandardAcceleratorInfo;
  isEditView: boolean;
  viewState: number;
  hasError: boolean;
  action: number;
  source: AcceleratorSource;
  protected statusMessage: string;
  private shortcutProvider: ShortcutProviderInterface;
  private lookupManager: AcceleratorLookupManager;

  constructor() {
    super();

    this.shortcutProvider = getShortcutProvider();

    this.lookupManager = AcceleratorLookupManager.getInstance();
  }

  protected onStatusMessageChanged(): void {
    if (this.statusMessage === '') {
      this.statusMessage = this.i18n('editViewStatusMessage');
    }
  }

  protected onEditButtonClicked(): void {
    this.viewState = ViewState.EDIT;
  }

  protected onDeleteButtonClicked(): void {
    this.shortcutProvider
        .removeAccelerator(
            this.source, this.action, getAccelerator(this.acceleratorInfo))
        .then((value: {result: AcceleratorResultData}) => {
          if (value.result.result === AcceleratorConfigResult.kSuccess) {
            if (this.shortcutProvider instanceof FakeShortcutProvider) {
              this.lookupManager.removeAccelerator(
                  this.source, this.action,
                  getAccelerator(this.acceleratorInfo));
            }

            this.dispatchEvent(new CustomEvent('request-update-accelerator', {
              bubbles: true,
              composed: true,
              detail: {source: this.source, action: this.action},
            }));
          }
        });
  }

  protected onCancelButtonClicked(): void {
    this.statusMessage = '';
    this.viewState = ViewState.VIEW;
  }

  protected showEditView(): boolean {
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
