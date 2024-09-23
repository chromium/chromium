// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './diagnostics_shared.css.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './diagnostics_sticky_banner.html.js';

export type ShowCautionBannerEvent = CustomEvent<{message: string}>;

declare global {
  interface HTMLElementEventMap {
    'show-caution-banner': ShowCautionBannerEvent;
    'dismiss-caution-banner': CustomEvent<void>;
  }
}

export class DiagnosticsStickyBannerElement extends PolymerElement {
  static get is(): string {
    return 'diagnostics-sticky-banner';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      bannerMessage: {
        type: String,
        value: '',
        notify: true,
      },

      scrollingClass: {
        type: String,
        value: '',
      },

      scrollTimerId: {
        type: Number,
        value: -1,
      },
    };
  }

  bannerMessage: string;
  protected scrollingClass: string;
  private scrollTimerId: number;

  override connectedCallback(): void {
    super.connectedCallback();
    window.addEventListener(
        'show-caution-banner',
        (e) => this.showCautionBannerHandler((e as CustomEvent)));
    window.addEventListener(
        'dismiss-caution-banner', this.dismissCautionBannerHandler);
    window.addEventListener('scroll', this.scrollClassHandler);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    window.removeEventListener(
        'show-caution-banner',
        (e) => this.showCautionBannerHandler((e as CustomEvent)));
    window.removeEventListener(
        'dismiss-caution-banner', this.dismissCautionBannerHandler);
    window.removeEventListener('scroll', this.scrollClassHandler);
  }

  /**
   * Event callback for 'show-caution-banner' which is triggered from routine-
   * section. Event will contain message to display on message property of
   * event found on path `event.detail.message`.
   */
  private showCautionBannerHandler = (e: ShowCautionBannerEvent): void => {
    assert(e.detail.message);
    this.bannerMessage = e.detail.message;
  };

  /**
   * Event callback for 'dismiss-caution-banner' which is triggered from
   * routine-section.
   */
  private dismissCautionBannerHandler = (): void => {
    this.bannerMessage = '';
  };

  /**
   * Event callback for 'scroll'.
   */
  private scrollClassHandler = (): void => {
    this.onScroll();
  };

  /**
   * Event handler for 'scroll' to ensure shadow and elevation of banner is
   * correct while scrolling. Timer is used to clear class after 300ms.
   */
  private onScroll(): void {
    if (!this.bannerMessage) {
      return;
    }

    // Reset timer since we've received another 'scroll' event.
    if (this.scrollTimerId !== -1) {
      this.scrollingClass = 'elevation-2';
      clearTimeout(this.scrollTimerId);
    }

    // Remove box shadow from banner since the user has stopped scrolling
    // for at least 300ms.
    this.scrollTimerId = window.setTimeout(() => this.scrollingClass = '', 300);
  }

  getScrollingClassForTesting(): string {
    return this.scrollingClass;
  }

  getScrollTimerIdForTesting(): number {
    return this.scrollTimerId;
  }

}

declare global {
  interface HTMLElementTagNameMap {
    'diagnostics-sticky-banner': DiagnosticsStickyBannerElement;
  }
}

customElements.define(
    DiagnosticsStickyBannerElement.is, DiagnosticsStickyBannerElement);
