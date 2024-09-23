// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Root element for the cellular setup flow. This element wraps
 * the psim setup flow, esim setup flow, and setup flow selection page.
 */
import './button_bar.js';
import './psim_flow_ui.js';
import './esim_flow_ui.js';

import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonBarElement} from './button_bar.js';
import {getTemplate} from './cellular_setup.html.js';
import {CellularSetupDelegate} from './cellular_setup_delegate.js';
import {ButtonBarState, CellularSetupPageName} from './cellular_types.js';
import {EsimFlowUiElement} from './esim_flow_ui.js';
import {PsimFlowUiElement} from './psim_flow_ui.js';

export interface CellularSetupElement {
  $: {
    buttonBar: ButtonBarElement,
  };
}

export class CellularSetupElement extends PolymerElement {
  static get is() {
    return 'cellular-setup' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,

      /**
       * Banner used in pSIM flow to show carrier network name. No banner
       * shown if the string is empty.
       */
      flowPsimBanner: {
        type: String,
        notify: true,
        value: '',
      },

      /**
       * Header for the flow, shown below the title. No header shown if the
       * string is empty.
       */
      flowHeader: {
        type: String,
        notify: true,
        value: '',
      },

      /**
       * Name of the currently displayed sub-page.
       */
      currentPageName: String,

      /**
       * Current user selected setup flow page name.
       */
      selectedFlow_: {
        type: String,
        value: null,
      },

      /**
       * Button bar button state.
       */
      buttonState_: {
        type: Object,
        notify: true,
      },

      /**
       * DOM Element corresponding to the visible page.
       */
      currentPage_: {
        type: Object,
        observer: 'onPageChange_',
      },

      /**
       * Text for the button_bar's 'Forward' button.
       */
      forwardButtonLabel_: {
        type: String,
      },
    };
  }

  delegate: CellularSetupDelegate;
  flowPsimBanner: string;
  flowHeader: string;
  currentPageName: CellularSetupPageName|null;
  private selectedFlow_: CellularSetupPageName|null;
  private buttonState_: ButtonBarState;
  private currentPage_: PsimFlowUiElement|EsimFlowUiElement;
  private forwardButtonLabel_: string;

  override connectedCallback() {
    super.connectedCallback();

    // By default eSIM flow is selected.
    if (!this.currentPageName) {
      this.currentPageName = CellularSetupPageName.ESIM_FLOW_UI;
    }
  }

  override ready() {
    super.ready();

    this.addEventListener('retry-requested', this.onRetryRequested_);
    this.addEventListener('forward-nav-requested', this.onForwardNavRequested_);
    this.addEventListener('cancel-requested', this.onCancelRequested_);
    this.addEventListener('focus-default-button', this.onFocusDefaultButton_);
  }

  private onPageChange_(): void {
    if (this.currentPage_) {
      this.flowPsimBanner = '';
      this.currentPage_.initSubflow();
    }
  }

  private onCancelRequested_(): void {
    this.dispatchEvent(new CustomEvent('exit-cellular-setup', {
      bubbles: true,
      composed: true,
    }));
  }

  private onRetryRequested_(): void {
    // TODO(crbug.com/40134918): Add try again logic.
  }

  private onForwardNavRequested_(): void {
    this.currentPage_.navigateForward();
  }

  private onFocusDefaultButton_(): void {
    afterNextRender(this, () => {
      // Try to focus on page elements before defaulting to focusing the button
      // bar.
      if (this.currentPage_.maybeFocusPageElement()) {
        return;
      }

      this.$.buttonBar.focusDefaultButton();
    });
  }

  private shouldShowPsimFlow_(currentPage: string): boolean {
    return currentPage === CellularSetupPageName.PSIM_FLOW_UI;
  }

  private shouldShowEsimFlow_(currentPage: string): boolean {
    return currentPage === CellularSetupPageName.ESIM_FLOW_UI;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CellularSetupElement.is]: CellularSetupElement;
  }
}

customElements.define(CellularSetupElement.is, CellularSetupElement);
