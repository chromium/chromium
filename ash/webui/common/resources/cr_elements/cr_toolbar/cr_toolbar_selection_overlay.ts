// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which displays the number of selected items with
 * Cancel/Delete buttons, designed to be used as an overlay on top of
 * <cr-toolbar>. See <history-toolbar> for an example usage.
 *
 * Note that the embedder is expected to set position: relative to make the
 * absolute positioning of this element work, and the cr-toolbar should have the
 * has-overlay attribute set when its overlay is shown to prevent access through
 * tab-traversal.
 *
 * Forked from
 * ui/webui/resources/
 *    cr_elements/cr_toolbar/cr_toolbar_selection_overlay.ts
 */

import '../cr_button/cr_button.js';
import '../cr_icon_button/cr_icon_button.js';
import '../cr_shared_vars.css.js';
import '../icons.html.js';

import {IronA11yAnnouncer} from '//resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {Debouncer, microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrButtonElement} from '../cr_button/cr_button.js';

import {getTemplate} from './cr_toolbar_selection_overlay.html.js';

export class CrToolbarSelectionOverlayElement extends PolymerElement {
  static get is() {
    return 'cr-toolbar-selection-overlay';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      show: {
        type: Boolean,
        observer: 'onShowChanged_',
        reflectToAttribute: true,
      },

      cancelLabel: String,
      selectionLabel: String,
      hasShown_: Boolean,
      selectionLabel_: String,
    };
  }

  static get observers() {
    return [
      'updateSelectionLabel_(show, selectionLabel)',
    ];
  }

  show: boolean;
  cancelLabel: string;
  selectionLabel: string;
  private hasShown_: boolean;
  private selectionLabel_: string;
  private debouncer_: Debouncer;

  override ready() {
    super.ready();
    this.setAttribute('role', 'toolbar');
  }

  get deleteButton(): CrButtonElement {
    return this.shadowRoot!.querySelector<CrButtonElement>('#delete')!;
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onClearSelectionClick_() {
    this.fire_('clear-selected-items');
  }

  private updateSelectionLabel_() {
    // Do this update in a microtask to ensure |show| and |selectionLabel|
    // are both updated.
    this.debouncer_ = Debouncer.debounce(this.debouncer_, microTask, () => {
      this.selectionLabel_ =
          this.show ? this.selectionLabel : this.selectionLabel_;
      this.setAttribute('aria-label', this.selectionLabel_);

      IronA11yAnnouncer.requestAvailability();
      this.fire_('iron-announce', {text: this.selectionLabel});
    });
  }

  private onShowChanged_() {
    if (this.show) {
      this.hasShown_ = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar-selection-overlay': CrToolbarSelectionOverlayElement;
  }
}

customElements.define(
    CrToolbarSelectionOverlayElement.is, CrToolbarSelectionOverlayElement);
