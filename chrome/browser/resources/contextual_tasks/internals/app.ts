// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';
import '//resources/cr_elements/cr_tab_box/cr_tab_box.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '/strings.m.js';

import type {CrSliderElement} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import type {EligibilityState, Tab} from '../contextual_tasks_internals.mojom-webui.js';
import {TabSelectionMode} from '../contextual_tasks_types.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

/**
 * Converts a mojo time to a JS time.
 */
function convertMojoTimeToJs(mojoTime: Time): Date {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

export class EventLogMessage {
  eventTime: Date;
  sourceLinkText: string = '';
  sourceLinkURL: string = '';
  message: string;

  constructor(
      eventTime: Time, sourceFile: string, sourceLine: bigint,
      message: string) {
    this.eventTime = convertMojoTimeToJs(eventTime);
    this.message = message;
    this.setSourceLink(sourceFile, sourceLine);
  }

  setSourceLink(sourceFile: string, sourceLine: bigint) {
    if (!sourceFile.startsWith('../../')) {
      this.sourceLinkText = `${sourceFile}(${sourceLine})`;
      return;
    }
    const fileName = sourceFile.slice(sourceFile.lastIndexOf('/') + 1);
    if (fileName.length === 0) {
      this.sourceLinkText = `${sourceFile}(${sourceLine})`;
      return;
    }
    this.sourceLinkText = `${fileName}(${sourceLine})`;
    this.sourceLinkURL =
        `https://source.chromium.org/chromium/chromium/src/+/main:${
            sourceFile.slice(6)};l=${sourceLine}`;
  }

  /**
   * Returns a string for dumping the message to logs.
   */
  toLogDump() {
    return `${this.eventTime}  ${this.sourceLinkText} ${this.message}`;
  }
}

export interface ContextualTasksInternalsAppElement {
  $: {
    tabSelectionModeSelect: HTMLSelectElement,
    minModelScoreSlider: CrSliderElement,
  };
}

export class ContextualTasksInternalsAppElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      relevantTabs_: {type: Array},
      query_: {type: String},
      isQueryPending_: {type: Boolean},
      tabSelectionMode_: {type: String},
      minModelScore_: {type: Number},
      eventLogMessages_: {type: Array},
      forcedHost_: {type: String},
      currentHost_: {type: String},
      eligibilityState_: {type: Object},
    };
  }

  protected accessor relevantTabs_: Tab[] = [];
  protected accessor query_: string = '';
  protected accessor isQueryPending_: boolean = false;
  protected accessor tabSelectionMode_: string = 'kStaticSignalsMlModel';
  protected accessor minModelScore_: number = 0.4;
  protected accessor eventLogMessages_: EventLogMessage[] = [];
  protected accessor forcedHost_: string = '';
  protected accessor currentHost_: string = '';
  protected accessor eligibilityState_: EligibilityState|null = null;

  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.proxy_.callbackRouter.onLogMessageAdded.addListener(
        this.onLogMessageAdded_.bind(this));
    this.refreshCurrentHost_();
    this.refreshEligibility_();
    this.syncTabsWithUrlHash_();
  }

  private syncTabsWithUrlHash_() {
    const tabUrlHashes = ['#model-selection', '#debugging', '#eligibility'];
    const tabBox = this.shadowRoot.querySelector('cr-tab-box')!;

    tabBox.addEventListener('selected-index-change', e => {
      window.location.hash = tabUrlHashes[e.detail] || '';
    });

    if (window.location.hash.startsWith('#')) {
      const entryIndex = tabUrlHashes.indexOf(window.location.hash);
      if (entryIndex >= 0) {
        tabBox.setAttribute('selected-index', String(entryIndex));
      }
    }
  }

  private async refreshEligibility_() {
    const response = await this.proxy_.handler.getEligibilityState();
    this.eligibilityState_ = response.state;
  }

  private async refreshCurrentHost_() {
    const response = await this.proxy_.handler.getForcedEmbeddedPageHost();
    const fullUrl = response.host;
    if (fullUrl) {
      // Create a URL object to extract just the host part for display.
      // If the URL is invalid, display an error message.
      const parsedUrl = URL.parse(fullUrl);
      this.currentHost_ = parsedUrl ?
          parsedUrl.host :
          `Error: Invalid URL provided (${fullUrl})`;
    } else {
      // Reset the current host if the forced host is empty.
      this.currentHost_ = '';
    }
  }

  protected onTabSelectionModeChange_() {
    this.tabSelectionMode_ = this.$.tabSelectionModeSelect.value;
    this.minModelScore_ =
        this.tabSelectionMode_ === 'kStaticSignalsMlModel' ? 0.4 : 0.8;
  }

  protected onMinModelScoreCrSliderValueChanged_() {
    this.minModelScore_ = this.$.minModelScoreSlider.value;
  }

  protected onQueryValueChanged_(e: CustomEvent<{value: string}>) {
    this.query_ = e.detail.value;
  }

  protected onForcedHostValueChanged_(e: CustomEvent<{value: string}>) {
    this.forcedHost_ = e.detail.value;
  }

  protected async onSetForcedHostClick_() {
    const url = URL.parse(this.forcedHost_);

    // Check if the current URL is valid.
    if (!url) {
      this.currentHost_ = `Error: Invalid URL provided (${this.forcedHost_})`;
      return;
    }

    // Verify that the host is a Google domain.
    // LINT.IfChange(AllowedHosts)
    if (!url.host.endsWith('.google.com') && !url.host.endsWith('.googlers.com')) {
      this.currentHost_ =
          `Error: URL must be a Google domain (.google.com or .googlers.com)`;
      return;
    }
    // LINT.ThenChange(//components/contextual_tasks/public/features.cc:AllowedHosts)

    await this.proxy_.handler.setForcedEmbeddedPageHost(url.href);
    await this.refreshCurrentHost_();
  }

  protected async onResetForcedHostClick_() {
    this.forcedHost_ = '';
    await this.proxy_.handler.setForcedEmbeddedPageHost('');
    await this.refreshCurrentHost_();
  }

  protected async onSubmitClick_() {
    this.eventLogMessages_ = [];
    this.isQueryPending_ = true;
    const response = await this.proxy_.handler.getRelevantContext({
      query: this.query_,
      tabSelectionMode: TabSelectionMode
          [this.tabSelectionMode_ as keyof typeof TabSelectionMode],
      minModelScore: this.minModelScore_,
    });
    this.relevantTabs_ = response.response.relevantTabs;
    this.isQueryPending_ = false;
  }

  private onLogMessageAdded_(
      eventTime: Time, sourceFile: string, sourceLine: bigint,
      message: string) {
    this.eventLogMessages_.push(
        new EventLogMessage(eventTime, sourceFile, sourceLine, message));
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-internals-app': ContextualTasksInternalsAppElement;
  }
}

customElements.define(
    ContextualTasksInternalsAppElement.is, ContextualTasksInternalsAppElement);
