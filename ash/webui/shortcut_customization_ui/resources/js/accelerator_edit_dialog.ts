// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_view.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {DomRepeat, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_edit_dialog.html.js';
import {ViewState} from './accelerator_view.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorInfo, AcceleratorSource, AcceleratorState} from './shortcut_types.js';

export interface AcceleratorEditDialogElement {
  $: {
    editDialog: CrDialogElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'accelerator-capturing-started': CustomEvent<void>;
    'accelerator-capturing-ended': CustomEvent<void>;
  }
}

/**
 * @fileoverview
 * 'accelerator-edit-dialog' is a dialog that displays the accelerators for
 * a given shortcut. Allows users to edit the accelerators.
 * TODO(jimmyxgong): Implement editing accelerators.
 */
const AcceleratorEditDialogElementBase = I18nMixin(PolymerElement);

export class AcceleratorEditDialogElement extends
    AcceleratorEditDialogElementBase {
  static get is(): string {
    return 'accelerator-edit-dialog';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      description: {
        type: String,
        value: '',
      },

      acceleratorInfos: {
        type: Array,
        value: () => [],
      },

      pendingNewAcceleratorState: {
        type: Number,
        value: ViewState.VIEW,
      },

      action: {
        type: Number,
        value: 0,
      },

      source: {
        type: Number,
        value: 0,
      },

      isAcceleratorCapturing: {
        type: Boolean,
        value: false,
      },
    };
  }

  description: string;
  acceleratorInfos: AcceleratorInfo[];
  action: number;
  source: AcceleratorSource;
  protected isAcceleratorCapturing: boolean;
  private pendingNewAcceleratorState: number;
  private acceleratorCapturingStartedListener = (): void =>
      this.onAcceleratorCapturingStarted();
  private acceleratorCapturingEndedListener = (): void =>
      this.onAcceleratorCapturingEnded();

  override connectedCallback(): void {
    super.connectedCallback();
    this.$.editDialog.showModal();

    window.addEventListener(
        'accelerator-capturing-started',
        this.acceleratorCapturingStartedListener);
    window.addEventListener(
        'accelerator-capturing-ended', this.acceleratorCapturingEndedListener);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    window.removeEventListener(
        'accelerator-capturing-started',
        this.acceleratorCapturingStartedListener);
    window.removeEventListener(
        'accelerator-capturing-ended', this.acceleratorCapturingEndedListener);
  }

  private getViewList(): DomRepeat {
    const viewList = this.shadowRoot!.querySelector('#viewList') as DomRepeat;
    assert(viewList);
    return viewList;
  }

  updateDialogAccelerators(updatedAccels: AcceleratorInfo[]): void {
    this.set('acceleratorInfos', []);
    this.getViewList().render();
    this.acceleratorInfos = updatedAccels;
  }

  protected onDoneButtonClicked(): void {
    this.$.editDialog.close();
  }

  protected onDialogClose(): void {
    this.dispatchEvent(
        new CustomEvent('edit-dialog-closed', {bubbles: true, composed: true}));
  }

  private onAcceleratorCapturingStarted(): void {
    this.isAcceleratorCapturing = true;
  }

  private onAcceleratorCapturingEnded(): void {
    this.isAcceleratorCapturing = false;
  }

  private focusAcceleratorItemContainer(): void {
    const editView = this.$.editDialog.querySelector('#pendingAccelerator');
    assert(editView);
    const accelItem = editView.shadowRoot!.querySelector('#acceleratorItem');
    assert(accelItem);
    const container =
        accelItem.shadowRoot!.querySelector<HTMLElement>('#container');
    assert(container);
    container!.focus();
  }

  protected onAddAcceleratorClicked(): void {
    this.pendingNewAcceleratorState = ViewState.ADD;

    // Flush the dom so that the AcceleratorEditView is ready to be focused.
    flush();
    this.focusAcceleratorItemContainer();
  }

  protected showAddButton(): boolean {
    // If the state is VIEW, no new pending accelerators are being added.
    return this.pendingNewAcceleratorState === ViewState.VIEW;
  }

  protected onRestoreDefaultButtonClicked(): void {
    getShortcutProvider()
        .restoreDefault(this.source, this.action)
        .then(({result}) => {
          // TODO(jimmyxgong): Potentially show partial resets as an error.
          if (result.result === AcceleratorConfigResult.kSuccess) {
            this.dispatchEvent(new CustomEvent('request-update-accelerator', {
              bubbles: true,
              composed: true,
              detail: {source: this.source, action: this.action},
            }));
          }
        });
  }

  protected getFilteredAccelerators(accelerators: AcceleratorInfo[]):
      AcceleratorInfo[] {
    return accelerators.filter(
        accel => accel.state !== AcceleratorState.kDisabledByUser);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-edit-dialog': AcceleratorEditDialogElement;
  }
}

customElements.define(
    AcceleratorEditDialogElement.is, AcceleratorEditDialogElement);
