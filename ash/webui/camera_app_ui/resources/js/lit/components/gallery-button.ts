// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './super-res-loading-indicator.js';

import {
  classMap,
  css,
  html,
  LitElement,
  PropertyDeclarations,
  styleMap,
} from 'chrome://resources/mwc/lit/index.js';

import {CoverPhoto} from '../../cover_photo.js';
import {I18nString} from '../../i18n_string.js';
import {getI18nMessage} from '../../models/load_time_data.js';
import {State} from '../../state.js';
import {withTooltip} from '../directives/with_tooltip.js';
import {StateObserverController} from '../state_observer_controller.js';
import {DEFAULT_STYLE} from '../styles.js';

export class GalleryButton extends LitElement {
  static override shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  static override styles = [
    DEFAULT_STYLE,
    css`
      #container {
        border-radius: 50%;
        height: var(--big-icon);
        position: relative;
        width: var(--big-icon);
      }

      #container.color {
        background-color: var(--cros-sys-illo-secondary);
      }

      button {
        border: 2px var(--cros-sys-primary_container) solid;
        border-radius: 50%;
        height: 100%;
        overflow: hidden;
        width: 100%;
      }

      button:enabled:active {
        transform: scale(1.07);
      }

      button.hidden {
        display: none;
      }

      #loading-indicator {
        height: 100%;
        position: absolute;
        width: 100%;
      }

      img {
        height: 100%;
        object-fit: cover;
        width: 100%;
      }

      img:not(.draggable) {
        pointer-events: none;
      }
    `,
  ];

  static override properties: PropertyDeclarations = {
    cover: {attribute: false},
  };

  cover: CoverPhoto|null = null;

  getCoverUrlForTesting(): string {
    return this.shadowRoot?.querySelector('img')?.getAttribute('src') ?? '';
  }

  private readonly superResZoomState =
      new StateObserverController(this, State.SUPER_RES_ZOOM);

  private readonly takingState =
      new StateObserverController(this, State.TAKING);

  private readonly timerTickState =
      new StateObserverController(this, State.TIMER_TICK);

  isSuperResZoomCapture(): boolean {
    return this.superResZoomState.value && this.takingState.value &&
        !this.timerTickState.value;
  }

  override render(): RenderResult {
    const buttonClasses = {
      hidden: this.cover === null,
    };
    const imgClasses = {
      draggable: this.cover?.draggable ?? false,
    };
    const containerClass = {
      color: this.isSuperResZoomCapture(),
    };
    return html`
    <div id="container" class=${classMap(containerClass)}>
      <super-res-loading-indicator id="loading-indicator"
        .photoProcessing=${this.takingState.value && !this.timerTickState.value}
        style=${styleMap({
      visibility: this.superResZoomState.value ? 'visible' : 'hidden',
    })}>
      </super-res-loading-indicator>
      <button
          aria-label=${getI18nMessage(I18nString.GALLERY_BUTTON)}
          ${withTooltip()}
          class=${classMap(buttonClasses)}>
        <img alt=""
            class=${classMap(imgClasses)}
            src=${this.cover?.url ?? ''}>
      </button>
    </div>
    `;
  }
}

window.customElements.define('gallery-button', GalleryButton);

declare global {
  interface HTMLElementTagNameMap {
    'gallery-button': GalleryButton;
  }
}
