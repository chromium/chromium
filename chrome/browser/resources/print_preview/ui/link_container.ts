// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './print_preview_vars.css.js';
import './throbber.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
// <if expr="is_win">
import {DestinationOrigin, GooglePromotedDestinationId} from '../data/destination.js';

// </if>
import {getTemplate} from './link_container.html.js';

export interface PrintPreviewLinkContainerElement {
  $: {
    // <if expr="is_macosx">
    openPdfInPreviewLink: HTMLDivElement,
    openPdfInPreviewThrobber: HTMLDivElement,
    // </if>
    systemDialogLink: HTMLDivElement,
    systemDialogThrobber: HTMLDivElement,
  };
}

export class PrintPreviewLinkContainerElement extends PolymerElement {
  static get is() {
    return 'print-preview-link-container';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appKioskMode: Boolean,

      destination: Object,

      disabled: Boolean,

      shouldShowSystemDialogLink_: {
        type: Boolean,
        computed:
            'computeShouldShowSystemDialogLink_(appKioskMode, destination)',
        reflectToAttribute: true,
      },

      systemDialogLinkDisabled_: {
        type: Boolean,
        computed: 'computeSystemDialogLinkDisabled_(disabled)',
      },

      openingSystemDialog_: {
        type: Boolean,
        value: false,
      },

      openingInPreview_: {
        type: Boolean,
        value: false,
      },
    };
  }

  appKioskMode: boolean;
  destination: Destination|null;
  disabled: boolean;
  private shouldShowSystemDialogLink_: boolean;
  private systemDialogLinkDisabled_: boolean;
  private openingSystemDialog_: boolean;
  private openingInPreview_: boolean;

  /**
   * @return Whether the system dialog link should be visible.
   */
  private computeShouldShowSystemDialogLink_(): boolean {
    if (this.appKioskMode) {
      return false;
    }
    // <if expr="not is_win">
    return true;
    // </if>
    // <if expr="is_win">
    return !!this.destination &&
        this.destination.origin === DestinationOrigin.LOCAL &&
        this.destination.id !== GooglePromotedDestinationId.SAVE_AS_PDF;
    // </if>
  }

  /**
   * @return Whether the system dialog link should be disabled
   */
  private computeSystemDialogLinkDisabled_(): boolean {
    // <if expr="not is_win">
    return false;
    // </if>
    // <if expr="is_win">
    return this.disabled;
    // </if>
  }

  private fire_(eventName: string) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true}));
  }


  private onSystemDialogClick_() {
    if (!this.shouldShowSystemDialogLink_) {
      return;
    }

    // <if expr="not is_win">
    this.openingSystemDialog_ = true;
    // </if>
    this.fire_('print-with-system-dialog');
  }

  // <if expr="is_macosx">
  private onOpenInPreviewClick_() {
    this.openingInPreview_ = true;
    this.fire_('open-pdf-in-preview');
  }
  // </if>

  /** @return Whether the system dialog link is available. */
  systemDialogLinkAvailable(): boolean {
    return this.shouldShowSystemDialogLink_ && !this.systemDialogLinkDisabled_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-link-container': PrintPreviewLinkContainerElement;
  }
}

customElements.define(
    PrintPreviewLinkContainerElement.is, PrintPreviewLinkContainerElement);
