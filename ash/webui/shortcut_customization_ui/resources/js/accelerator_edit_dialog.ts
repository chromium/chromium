// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_view.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {DomRepeat, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_edit_dialog.html.js';
import {ViewState} from './accelerator_view.js';
import {AcceleratorInfo, AcceleratorSource} from './shortcut_types.js';

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
export class AcceleratorEditDialogElement extends PolymerElement {
  static get is() {
    return 'accelerator-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      description: {
        type: String,
        value: '',
      },

      acceleratorInfos: {
        type: Array,
        value: () => {},
      },

      pendingNewAcceleratorState_: {
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

      isAcceleratorCapturing_: {
        type: Boolean,
        value: false,
      },
    };
  }

  description: string;
  acceleratorInfos: AcceleratorInfo[];
  action: number;
  source: AcceleratorSource;
  protected isAcceleratorCapturing_: boolean;
  private pendingNewAcceleratorState_: number;
  private acceleratorCapturingStartedListener_ = () =>
      this.onAcceleratorCapturingStarted_();
  private acceleratorCapturingEndedListener_ = () =>
      this.onAcceleratorCapturingEnded_();

  override connectedCallback() {
    super.connectedCallback();
    this.$.editDialog.showModal();

    window.addEventListener(
        'accelerator-capturing-started',
        this.acceleratorCapturingStartedListener_);
    window.addEventListener(
        'accelerator-capturing-ended', this.acceleratorCapturingEndedListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener(
        'accelerator-capturing-started',
        this.acceleratorCapturingStartedListener_);
    window.removeEventListener(
        'accelerator-capturing-ended', this.acceleratorCapturingEndedListener_);
  }

  private getViewList_(): DomRepeat {
    const viewList = this.shadowRoot!.querySelector('#viewList') as DomRepeat;
    assert(viewList);
    return viewList;
  }

  updateDialogAccelerators(updatedAccels: AcceleratorInfo[]) {
    this.set('acceleratorInfos', []);
    this.getViewList_().render();
    this.acceleratorInfos = updatedAccels;
  }

  protected onDoneButtonClicked_() {
    this.$.editDialog.close();
  }

  protected onDialogClose_() {
    this.dispatchEvent(
        new CustomEvent('edit-dialog-closed', {bubbles: true, composed: true}));
  }

  private onAcceleratorCapturingStarted_() {
    this.isAcceleratorCapturing_ = true;
  }

  private onAcceleratorCapturingEnded_() {
    this.isAcceleratorCapturing_ = false;
  }

  private focusAcceleratorItemContainer_() {
    const editView = this.$.editDialog.querySelector('#pendingAccelerator');
    assert(editView);
    const accelItem = editView.shadowRoot!.querySelector('#acceleratorItem');
    assert(accelItem);
    const container =
        accelItem.shadowRoot!.querySelector<HTMLElement>('#container');
    assert(container);
    container!.focus();
  }

  protected onAddAcceleratorClicked_() {
    this.pendingNewAcceleratorState_ = ViewState.ADD;

    // Flush the dom so that the AcceleratorEditView is ready to be focused.
    flush();
    this.focusAcceleratorItemContainer_();
  }

  protected showAddButton_(): boolean {
    // If the state is VIEW, no new pending accelerators are being added.
    return this.pendingNewAcceleratorState_ === ViewState.VIEW;
  }

  protected onRestoreDefaultButtonClicked_() {
    // TODO(jimmyxgong): Implement this function.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-edit-dialog': AcceleratorEditDialogElement;
  }
}

customElements.define(
    AcceleratorEditDialogElement.is, AcceleratorEditDialogElement);
