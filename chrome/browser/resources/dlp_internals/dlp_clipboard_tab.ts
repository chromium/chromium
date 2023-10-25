// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './dlp_clipboard_tab.html.js';
import {DataTransferEndpoint, EndpointType, PageHandler, PageHandlerInterface} from './dlp_internals.mojom-webui.js';

const EndpointTypeMap = {
  [EndpointType.kDefault]: 'Default',
  [EndpointType.kUrl]: 'URL',
  [EndpointType.kClipboardHistory]: 'Clipboard History',
  [EndpointType.kUnknownVm]: 'Unknown VM',
  [EndpointType.kArc]: 'Arc',
  [EndpointType.kBorealis]: 'Borealis',
  [EndpointType.kCrostini]: 'Crostini',
  [EndpointType.kPluginVm]: 'Plugin VM',
  [EndpointType.kLacros]: 'Lacros',
};

export class DlpClipboardElement extends CustomElement {
  static get is() {
    return 'dlp-clipboard-tab';
  }

  static override get template() {
    return getTemplate();
  }

  set clipboardSourceTypeString(clipboardSourceTypeString: string) {
    this.setValueToElement('#clipboard-source-type', clipboardSourceTypeString);
  }

  set clipboardSourceUrlString(clipboardSourceUrlString: string) {
    this.setValueToElement('#clipboard-source-url', clipboardSourceUrlString);
  }

  private readonly pageHandler: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler = PageHandler.getRemote();
  }

  connectedCallback() {
    this.fetchClipboardSourceInfo();
  }

  private setClipboardInfo(source: DataTransferEndpoint|null|undefined) {
    if (!source) {
      this.clipboardSourceTypeString = 'undefined';
      this.clipboardSourceUrlString = 'undefined';
      return;
    }

    this.clipboardSourceTypeString =
        `${this.endpointTypeToString(source.type)}`;
    if (source.url === undefined) {
      this.clipboardSourceUrlString = 'undefined';
    } else {
      this.clipboardSourceUrlString = source.url.url;
    }
  }

  private async fetchClipboardSourceInfo(): Promise<void> {
    this.pageHandler.getClipboardDataSource()
        .then((value: {source: DataTransferEndpoint|null}) => {
          this.setClipboardInfo(value.source);
        })
        .catch((e: object) => {
          console.warn(`getClipboardDataSource failed: ${JSON.stringify(e)}`);
        });
  }

  private setValueToElement(elementId: string, stringValue: string) {
    const htmlElement = (this.$(elementId) as HTMLElement);
    if (htmlElement) {
      htmlElement.innerText = stringValue;
    } else {
      console.error(`Could not find ${elementId} element.`);
    }
  }

  private endpointTypeToString(type: EndpointType): string {
    return EndpointTypeMap[type] || 'invalid';
  }
}

customElements.define(DlpClipboardElement.is, DlpClipboardElement);
