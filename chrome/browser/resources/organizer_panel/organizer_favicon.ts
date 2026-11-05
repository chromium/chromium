// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_favicon.css.js';
import {getHtml} from './organizer_favicon.html.js';

const NTP_HOSTS = new Set<string>([
  'new-tab-page',
  'newtab',
  // Used for the split view NTP.
  'tab-search.top-chrome',
]);

export function isNtpUrl(url: string): boolean {
  if (!url) {
    return false;
  }
  try {
    const parsed = new URL(url);
    return parsed.protocol === 'chrome:' && NTP_HOSTS.has(parsed.hostname);
  } catch {
    return false;
  }
}

export interface OrganizerFaviconElement {
  $: {
    favicons: HTMLElement,
    firstFavicon: HTMLElement,
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

  protected isNtpUrl_(url: string): boolean {
    return isNtpUrl(url);
  }

  protected getFaviconStyle_(url: string): string {
    if (!url) {
      return '';
    }
    const favicon = getFaviconForPageURL(url, false);
    const property = isNtpUrl(url) ? 'mask-image' : 'background-image';
    return `${property}: ${favicon};`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-favicon': OrganizerFaviconElement;
  }
}

customElements.define(OrganizerFaviconElement.is, OrganizerFaviconElement);
