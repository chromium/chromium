// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_favicon.css.js';
import {getHtml} from './organizer_favicon.html.js';

export interface OrganizerFaviconElement {
  $: {
    favicons: HTMLElement,
    firstFavicon: HTMLElement,
    secondFavicon: HTMLElement,
  };
}

export class OrganizerFaviconElement extends CrLitElement {
  static get is() {
    return 'organizer-favicon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      url: {type: String},
      secondaryUrl: {type: String},
      stackVertically: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor url: string = '';
  accessor secondaryUrl: string = '';
  accessor stackVertically: boolean = false;

  protected getFavicon_(url: string): string {
    return getFaviconForPageURL(url, false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-favicon': OrganizerFaviconElement;
  }
}

customElements.define(OrganizerFaviconElement.is, OrganizerFaviconElement);
