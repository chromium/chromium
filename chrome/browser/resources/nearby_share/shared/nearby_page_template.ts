// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-page-template is used as a template for pages. It
 * provide a consistent setup for all pages with title, sub-title, body slot
 * and button options.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
// <if expr='chromeos_ash'>
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';

// </if>

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_page_template.html.js';
import {CloseReason} from './types.js';

export class NearbyPageTemplateElement extends PolymerElement {
  static get is() {
    return 'nearby-page-template' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      title: {
        type: String,
      },

      subTitle: {
        type: String,
      },

      /**
       * Alternate subtitle for screen readers. If not falsey, then the
       * #pageSubTitle is aria-hidden and the #a11yAnnouncedPageSubTitle is
       * rendered on screen readers instead. Changes to this value will result
       * in aria-live announcements.
       */
      a11yAnnouncedSubTitle: {
        type: String,
        value: null,
      },

      /**
       * Text to show on the action button. If either this is falsey, or if
       * |closeOnly| is true, then the action button is hidden.
       */
      actionButtonLabel: {
        type: String,
      },

      actionButtonEventName: {type: String, value: 'action'},

      actionDisabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Text to show on the cancel button. If either this is falsey, or if
       * |closeOnly| is true, then the cancel button is hidden.
       */
      cancelButtonLabel: {
        type: String,
      },

      cancelButtonEventName: {
        type: String,
        value: 'cancel',
      },

      /**
       * Text to show on the utility button. If either this is falsey, or if
       * |closeOnly| is true, then the utility button is hidden.
       */
      utilityButtonLabel: {
        type: String,
      },

      /**
       * When true, shows the open-in-new icon to the left of the button label.
       */
      utilityButtonOpenInNew: {
        type: Boolean,
        value: false,
      },

      utilityButtonEventName: {
        type: String,
        value: 'utility',
      },

      /**
       * When true, hide all other buttons and show a close button.
       */
      closeOnly: {
        type: Boolean,
        value: false,
      },
    };
  }

  a11yAnnouncedSubTitle: string|null;
  actionButtonEventName: string;
  actionButtonLabel: string|null;
  actionDisabled: boolean;
  cancelButtonEventName: string;
  cancelButtonLabel: string|null;
  closeOnly: boolean;
  subTitle: string|null;
  utilityButtonLabel: string|null;
  utilityButtonEventName: string;
  utilityButtonOpenInNew: boolean;

  private fire_(eventName: string, detail?: unknown): void {
    this.dispatchEvent(new CustomEvent(
        eventName, {bubbles: true, composed: true, detail: detail || {}}));
  }

  private onActionClick_(): void {
    this.fire_(this.actionButtonEventName);
  }

  private onCancelClick_(): void {
    this.fire_(this.cancelButtonEventName);
  }

  private onUtilityClick_(): void {
    this.fire_(this.utilityButtonEventName);
  }

  private onCloseClick_(): void {
    this.fire_('close', {reason: CloseReason.UNKNOWN});
  }

  private getDialogAriaLabelledBy_(): string {
    let labelIds = 'pageTitle';
    if (!this.a11yAnnouncedSubTitle) {
      labelIds += ' pageSubTitle';
    }
    return labelIds;
  }

  private getSubTitleAriaHidden_(): string|undefined {
    if (this.a11yAnnouncedSubTitle) {
      return 'true';
    }
    return undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyPageTemplateElement.is]: NearbyPageTemplateElement;
  }
}

customElements.define(NearbyPageTemplateElement.is, NearbyPageTemplateElement);
