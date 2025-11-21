// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './setup_list.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SetupListElement} from './setup_list.js';
import {getCss} from './setup_list_module_wrapper.css.js';
import {getHtml} from './setup_list_module_wrapper.html.js';

export type UndoActionEvent =
    CustomEvent<{message: string, restoreCallback?: () => void}>;
export type ReadyEvent = CustomEvent<boolean>;

declare global {
  interface HTMLElementEventMap {
    'disable-module': UndoActionEvent;
    'dismiss-module-instance': UndoActionEvent;
    'module-ready': ReadyEvent;
  }
}

export interface SetupListModuleWrapperElement {
  $: {
    container: HTMLElement,
    moduleElement: HTMLElement,
    setupList: SetupListElement,
    undoToast: CrToastElement,
    undoToastMessage: HTMLElement,
  };
}

/** Faux module container for the NTP Setup List. */
export class SetupListModuleWrapperElement extends CrLitElement {
  static get is() {
    return 'setup-list-module-wrapper';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      moduleHidden_: {type: Boolean},
      moduleReady_: {type: Boolean},

      /** Data about the most recent un-doable action. */
      undoData_: {type: Object},

      maxPromos: {type: Number, attribute: true, useDefault: true},
      maxCompletedPromos: {type: Number, attribute: true, useDefault: true},
    };
  }

  accessor maxPromos: number = 0;
  accessor maxCompletedPromos: number = 0;

  protected accessor moduleHidden_: boolean = false;
  protected accessor moduleReady_: boolean = false;
  protected accessor undoData_: {message: string, undo?: () => void}|null =
      null;

  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    this.eventTracker_.add(window, 'keydown', this.onWindowKeydown_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected onHideModule_(e: UndoActionEvent) {
    this.moduleHidden_ = true;
    const restoreCallback = e.detail.restoreCallback;
    this.undoData_ = {
      message: e.detail.message,
      undo: () => {
        this.moduleHidden_ = false;
        if (restoreCallback) {
          restoreCallback();
        }
      },
    };
    this.$.undoToast.show();
  }

  protected onModuleReady_(e: CustomEvent) {
    this.moduleReady_ = e.detail;
  }

  protected isModuleHidden_(): boolean {
    return !this.moduleReady_ || this.moduleHidden_;
  }

  protected onUndoButtonClick_() {
    if (!this.undoData_) {
      return;
    }

    // Restore to the previous state.
    this.undoData_.undo!();

    // Notify the user.
    this.$.undoToast.hide();
    this.undoData_ = null;
  }

  private onWindowKeydown_(e: KeyboardEvent) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.key === 'z') {
      this.onUndoButtonClick_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'setup-list-module-wrapper': SetupListModuleWrapperElement;
  }
}

customElements.define(
    SetupListModuleWrapperElement.is, SetupListModuleWrapperElement);
