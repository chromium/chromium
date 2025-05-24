// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_chip/cr_chip.js';

import {assert} from '//resources/js/assert.js';
import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {PageClassification} from './omnibox.mojom-webui.js';
import {getCss} from './request.css.js';
import {getHtml} from './request.html.js';
import type {Request} from './suggest_internals.mojom-webui.js';
import {RequestStatus} from './suggest_internals.mojom-webui.js';

// Displays a suggest request and its response.
export class SuggestRequestElement extends CrLitElement {
  static get is() {
    return 'suggest-request';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      request: {type: Object},
      requestDataJson_: {type: String},
      responseJson_: {type: String},
      pgcl_: {type: String},
      expanded_: {type: Boolean},
    };
  }

  accessor request: Request|null = null;
  protected accessor requestDataJson_: string = '';
  protected accessor responseJson_: string = '';
  private accessor pgcl_: string = '';
  protected accessor expanded_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (this.request === null) {
      this.requestDataJson_ = '';
      this.responseJson_ = '';
      this.pgcl_ = '';
      return;
    }

    this.requestDataJson_ = this.computeRequestDataJson_();
    this.responseJson_ = this.computeResponseJson_();
    this.pgcl_ = this.computePageClassification_();
  }

  private computeRequestDataJson_(): string {
    assert(this.request);
    try {
      // Try to parse the request body, if any.
      this.request.data['Request-Body'] =
          JSON.parse(this.request.data['Request-Body']!);
    } finally {
      // Pretty-print the parsed JSON.
      return JSON.stringify(this.request.data, null, 2);
    }
  }

  private computeResponseJson_(): string {
    assert(this.request);
    try {
      // Remove the magic XSSI guard prefix, if any, to get a valid JSON.
      const validJson = this.request.response.replace(')]}\'', '').trim();
      // Try to parse the valid JSON.
      const parsedJson = JSON.parse(validJson);
      // Pretty-print the parsed JSON.
      return JSON.stringify(parsedJson, null, 2);
    } catch (e) {
      return '';
    }
  }

  private computePageClassification_(): string {
    assert(this.request);

    if (!this.request.url.url) {
      return '';
    }
    // Find pgcl value in request url.
    const url = new URL(this.request.url.url);
    const queryMatches = url.search.match(/pgcl=(?<pgcl>[^&]*)/);
    // If no pgcl value in request, set pgcl to empty
    if (queryMatches === null || !queryMatches.groups) {
      return '';
    }

    return queryMatches.groups['pgcl'] || '';
  }

  protected getPageClassificationLabel_(): string {
    return PageClassification[parseInt(this.pgcl_)]!;
  }

  private insertTextProtoLinks_(stringJSON: string): string {
    // Create regex to match against strings of desired form
    // Ex. "google:entityinfo" : "<base64 encoding>".
    // Extract the type (groups or entity) and proto (base64 encoding).
    const regexGroups =
        /"(?<type>(?:google:entityinfo|google:groupsinfo|X-Client-Data))":\s"(?<proto>(?:[A-Za-z0-9+\/]{4})*(?:[A-Za-z0-9+\/]{2}==|[A-Za-z0-9+\/]{3}=)?)"/g;
    // Replace base64 groupinfo or entityinfo encodings with links to
    // textproto in protoshop and return final string.
    return stringJSON.replace(
        regexGroups, (_match, _p1, _p2, _offset, _string, groups) => {
          let urlType = '';
          switch (groups.type) {
            case 'google:entityinfo':
              urlType = 'gws.searchbox.chrome.EntityInfo';
              break;
            case 'google:groupsinfo':
              urlType = 'gws.searchbox.chrome.GroupsInfo';
              break;
            case 'X-Client-Data':
              urlType = 'webserver.gws.ClientDataHeader';
          }
          return `"${
              groups
                  .type}": <a target='_blank' href=https://protoshop.corp.google.com/embed?tabs=textproto&type=${
              urlType}&protobytes=${groups.proto}>${groups.proto}</a>`;
        });
  }

  protected getRequestDataHtml_(): TrustedHTML {
    const htmlJSON = this.insertTextProtoLinks_(this.requestDataJson_);
    return sanitizeInnerHtml(htmlJSON);
  }

  protected getRequestPath_(): string {
    if (this.request === null) {
      return '';
    }

    try {
      const url = new URL(this.request.url.url);
      const queryMatches = url.search.match(/(q|delq)=[^&]*/);
      return url.pathname + '?' + (queryMatches ? queryMatches[0] : '');
    } catch (e) {
      return '';
    }
  }

  protected getResponseHtml_(): TrustedHTML {
    const htmlJSON = this.insertTextProtoLinks_(this.responseJson_);
    return sanitizeInnerHtml(htmlJSON);
  }

  protected getStatusIcon_(): string {
    switch (this.request?.status) {
      case RequestStatus.kHardcoded:
        return 'suggest:lock';
      case RequestStatus.kCreated:
        return 'cr:create';
      case RequestStatus.kSent:
        return 'cr:schedule';
      case RequestStatus.kSucceeded:
        return 'cr:check-circle';
      case RequestStatus.kFailed:
        return 'cr:cancel';
      default:
        return '';
    }
  }

  protected getStatusTitle_(): string {
    switch (this.request?.status) {
      case RequestStatus.kHardcoded:
        return 'hardcoded';
      case RequestStatus.kCreated:
        return 'created';
      case RequestStatus.kSent:
        return 'pending';
      case RequestStatus.kSucceeded:
        const startTimeMs = Number(this.request.startTime.internalValue) / 1000;
        const endTimeMs = Number(this.request.endTime.internalValue) / 1000;
        return `succeeded in ${Math.round(endTimeMs - startTimeMs)} ms`;
      case RequestStatus.kFailed:
        return 'failed';
      default:
        return '';
    }
  }

  protected getTimestamp_(): string {
    if (this.request === null) {
      return '';
    }

    // The JS Date() is based off of the number of milliseconds since the
    // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
    // base::Time (represented in mojom.Time) represents the number of
    // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
    // This computes the final JS time by computing the epoch delta and the
    // conversion from microseconds to milliseconds.
    const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
    const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
    // |epochDeltaMs| is equivalent to base::Time::kTimeTToMicrosecondsOffset.
    const epochDeltaMs = unixEpoch - windowsEpoch;
    const startTimeMs = Number(this.request.startTime.internalValue) / 1000;
    return (new Date(startTimeMs - epochDeltaMs)).toLocaleTimeString();
  }

  protected onCopyRequestClick_() {
    navigator.clipboard.writeText(this.requestDataJson_);

    this.dispatchEvent(new CustomEvent('show-toast', {
      bubbles: true,
      composed: true,
      detail: 'Request Copied to Clipboard',
    }));
  }

  protected onCopyResponseClick_() {
    navigator.clipboard.writeText(this.responseJson_);

    this.dispatchEvent(new CustomEvent('show-toast', {
      bubbles: true,
      composed: true,
      detail: 'Response Copied to Clipboard',
    }));
  }

  protected onHardcodeResponseClick_() {
    this.dispatchEvent(new CustomEvent('open-hardcode-response-dialog', {
      bubbles: true,
      composed: true,
      detail: this.responseJson_,
    }));
  }

  protected onChipClick_(e: CustomEvent<string>) {
    this.dispatchEvent(new CustomEvent('chip-click', {
      bubbles: true,
      composed: true,
      detail: this.pgcl_,
    }));
    // Allow chip to be found with aria label (originally hidden).
    const button =
        this.shadowRoot.querySelector<HTMLElement>('cr-expand-button')!;
    const label = button.shadowRoot!.querySelector<HTMLElement>('#label')!;
    label.ariaHidden = 'false';
    // Prevent cr-expand-button from being clicked when chip is clicked.
    e.stopPropagation();
    e.preventDefault();
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }
}

export type RequestElement = SuggestRequestElement;

declare global {
  interface HTMLElementTagNameMap {
    'suggest-request': SuggestRequestElement;
  }
}

customElements.define(SuggestRequestElement.is, SuggestRequestElement);
