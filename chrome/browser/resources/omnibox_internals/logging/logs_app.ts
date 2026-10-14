// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {getCss} from './logs_app.css.js';
import {getHtml} from './logs_app.html.js';
import {BrowserProxy} from './logs_browser_proxy.js';

/**
 * Converts a mojo time to a JS time.
 */
function convertMojoTimeToJs(mojoTime: Time): Date {
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

/**
 * Get a Chromium source link given a file name and line number.
 */
function getSourceLinkUrl(sourceFile: string, sourceLine: number): string {
  return `https://source.chromium.org/chromium/chromium/src/+/main:${
      sourceFile.slice(6)};l=${sourceLine}`;
}

/**
 * Get the text for a Chromium source link URL.
 */
function getSourceLinkText(sourceFile: string, sourceLine: number): string {
  if (!sourceFile.startsWith('../../')) {
    return `${sourceFile}(${sourceLine})`;
  }
  const fileName = sourceFile.slice(sourceFile.lastIndexOf('/') + 1);
  if (fileName.length === 0) {
    return `${sourceFile}(${sourceLine})`;
  }
  return `${fileName} (${sourceLine})`;
}

interface EventLogMessage {
  eventTime: Date;
  tag: string;
  sourceLinkText: string;
  sourceLinkUrl: string;
  fileName: string;
  message: string;
  protoType: string|null;
  protoBase64: string|null;
}

export class LogsAppElement extends CrLitElement {
  static get is() {
    return 'logs-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      eventLogMessages_: {type: Array},
      selectedTags_: {type: Array},
      selectedSources_: {type: Array},
      selectedProtos_: {type: Array},
      openFilterDropdown_: {type: String},
      uniqueTags_: {type: Object},
      uniqueSources_: {type: Object},
      uniqueProtos_: {type: Object},
    };
  }

  protected accessor eventLogMessages_: EventLogMessage[] = [];
  protected accessor selectedTags_: string[] = [];
  protected accessor selectedSources_: string[] = [];
  protected accessor selectedProtos_: string[] = [];
  protected accessor openFilterDropdown_: string|null = null;

  protected accessor uniqueTags_ = new Set<string>();
  protected accessor uniqueSources_ = new Set<string>();
  protected accessor uniqueProtos_ = new Set<string>();

  protected get filteredMessages_(): EventLogMessage[] {
    return this.eventLogMessages_.filter(item => {
      const matchTag = this.selectedTags_.length === 0 ||
          this.selectedTags_.includes(item.tag);
      const matchSource = this.selectedSources_.length === 0 ||
          this.selectedSources_.includes(item.fileName);
      const matchProto = this.selectedProtos_.length === 0 ||
          (item.protoType !== null &&
           this.selectedProtos_.includes(item.protoType));
      return matchTag && matchSource && matchProto;
    });
  }

  protected onFilterChange_ = (e: Event) => {
    const checkbox = e.target as HTMLInputElement;
    const value = checkbox.value;
    const filter = checkbox.dataset['filter'];

    if (!filter) {
      return;
    }

    if (filter === 'tag') {
      this.selectedTags_ = checkbox.checked ?
          [...this.selectedTags_, value] :
          this.selectedTags_.filter(t => t !== value);
    } else if (filter === 'source') {
      this.selectedSources_ = checkbox.checked ?
          [...this.selectedSources_, value] :
          this.selectedSources_.filter(s => s !== value);
    } else if (filter === 'proto') {
      this.selectedProtos_ = checkbox.checked ?
          [...this.selectedProtos_, value] :
          this.selectedProtos_.filter(p => p !== value);
    }
  };

  protected toggleDropdown_(filterName: string) {
    if (this.openFilterDropdown_ === filterName) {
      this.openFilterDropdown_ = null;
    } else {
      this.openFilterDropdown_ = filterName;
    }
  }

  protected onDropdownToggleClick_(e: Event) {
    const button = e.currentTarget as HTMLButtonElement;
    const filterName = button.dataset['filter'];
    if (filterName) {
      this.toggleDropdown_(filterName);
    }
  }


  private proxy_: BrowserProxy = BrowserProxy.getInstance();
  private listenerIds_: number[] = [];

  private onWindowClick_ = (e: Event) => {
    if (!this.openFilterDropdown_) {
      return;
    }
    const path = e.composedPath();
    const clickedInside = path.some(target => {
      if (target instanceof HTMLElement) {
        if (target.classList.contains('dropdown-content')) {
          return true;
        }
        if (target.tagName === 'BUTTON' &&
            target.dataset['filter'] === this.openFilterDropdown_) {
          return true;
        }
      }
      return false;
    });

    if (!clickedInside) {
      this.openFilterDropdown_ = null;
    }
  };

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds_ =
        [this.proxy_.getCallbackRouter().onLogMessageAdded.addListener(
            this.onLogMessageAdded_.bind(this))];
    window.addEventListener('click', this.onWindowClick_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.proxy_.getCallbackRouter().removeListener(id));
    this.listenerIds_ = [];
    window.removeEventListener('click', this.onWindowClick_);
  }

  private onLogMessageAdded_(
      eventTime: Time, tag: string, sourceFile: string, sourceLine: number,
      message: string, protoType: string|null, protoBase64: string|null) {
    const lastSlash = sourceFile.lastIndexOf('/');
    const fileName =
        lastSlash !== -1 ? sourceFile.slice(lastSlash + 1) : sourceFile;

    this.eventLogMessages_.push({
      eventTime: convertMojoTimeToJs(eventTime),
      tag,
      sourceLinkText: getSourceLinkText(sourceFile, sourceLine),
      sourceLinkUrl: getSourceLinkUrl(sourceFile, sourceLine),
      fileName,
      message,
      protoType,
      protoBase64,
    });

    this.uniqueTags_.add(tag);
    this.uniqueSources_.add(fileName);
    if (protoType) {
      this.uniqueProtos_.add(protoType);
    }

    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'logs-app': LogsAppElement;
  }
}

customElements.define(LogsAppElement.is, LogsAppElement);
