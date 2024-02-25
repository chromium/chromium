// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  classMap,
  css,
  html,
  LitElement,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {CoverPhoto} from '../../cover_photo.js';
import {I18nString} from '../../i18n_string.js';
import {getI18nMessage} from '../../models/load_time_data.js';
import {withTooltip} from '../directives/with_tooltip.js';
import {DEFAULT_STYLE} from '../styles.js';

export class GalleryButton extends LitElement {
  static override shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  static override styles = [
    DEFAULT_STYLE,
    css`
      button {
        border: 2px var(--cros-sys-primary_container) solid;
        border-radius: 50%;
        height: var(--big-icon);
        overflow: hidden;
        width: var(--big-icon);
      }

      button:enabled:active {
        transform: scale(1.07);
      }

      button.hidden {
        display: none;
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

  getCoverURLForTesting(): string {
    return this.shadowRoot?.querySelector('img')?.getAttribute('src') ?? '';
  }

  override render(): RenderResult {
    const buttonClasses = {
      hidden: this.cover === null,
    };
    const imgClasses = {
      draggable: this.cover?.draggable ?? false,
    };
    return html`
      <button
          aria-label=${getI18nMessage(I18nString.GALLERY_BUTTON)}
          ${withTooltip()}
          class=${classMap(buttonClasses)}>
        <img alt=""
            class=${classMap(imgClasses)}
            src=${this.cover?.url ?? ''}>
      </button>
    `;
  }
}

window.customElements.define('gallery-button', GalleryButton);

declare global {
  interface HTMLElementTagNameMap {
    /* eslint-disable-next-line @typescript-eslint/naming-convention */
    'gallery-button': GalleryButton;
  }
}
