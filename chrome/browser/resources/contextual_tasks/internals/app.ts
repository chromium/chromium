// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';
import '//resources/cr_elements/cr_tab_box/cr_tab_box.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '/strings.m.js';
import './session_state_tab.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {CrSliderElement} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {isMac} from '//resources/js/platform.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {browserProxyFactory} from '../contextual_tasks_internals.mojom-webui.js';
import type {BrowserProxy, EligibilityState, Tab} from '../contextual_tasks_internals.mojom-webui.js';
import {TabSelectionMode} from '../contextual_tasks_types.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {ContextualTasksSessionStateTabElement} from './session_state_tab.js';

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
  // |epochDeltaInMs| equals to base::Time::kMicrosecondsFromWindowsToUnixEpoch.
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
    sessionStateTab: ContextualTasksSessionStateTabElement,
    forcedHostInput: CrInputElement,
    setForcedHostButton: HTMLElement,
    clearForcedHostButton: HTMLElement,
    resetAllOverridesButton: HTMLElement,
    paramKeyInput: CrInputElement,
    paramValueInput: CrInputElement,
    addForcedParamButton: HTMLElement,
    effectiveUrlDisplay: HTMLElement,
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
      forcedHostError_: {type: String},
      paramKey_: {type: String},
      paramValue_: {type: String},
      currentParams_: {type: Object},
      forcedParamsError_: {type: String},
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
  protected accessor forcedHostError_: string = '';
  protected accessor paramKey_: string = '';
  protected accessor paramValue_: string = '';
  private accessor currentParams_: {[key: string]: string} = {};
  protected accessor forcedParamsError_: string = '';
  protected accessor eligibilityState_: EligibilityState|null = null;

  private proxy_: BrowserProxy = browserProxyFactory.getInstance();
  private keydownListener_:
      (e: KeyboardEvent) => void = this.onKeydown_.bind(this);

  override connectedCallback() {
    super.connectedCallback();
    this.proxy_.callbackRouter.onLogMessageAdded.addListener(
        this.onLogMessageAdded_.bind(this));
    document.addEventListener('keydown', this.keydownListener_);
    this.refreshCurrentHost_();
    this.refreshEligibility_();
    this.syncTabsWithUrlHash_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('keydown', this.keydownListener_);
  }

  private refreshAll_() {
    this.refreshCurrentHost_();
    this.refreshEligibility_();
    this.$.sessionStateTab.refresh();
  }

  // Refreshes the page state in place on Cmd+R (Mac) or Ctrl+R (other
  // platforms) instead of reloading the page, which would reset the event log.
  private onKeydown_(e: KeyboardEvent) {
    const hasPrimaryModifier =
        isMac ? (e.metaKey && !e.ctrlKey) : (e.ctrlKey && !e.metaKey);
    if (e.key.toLowerCase() !== 'r' || !hasPrimaryModifier || e.shiftKey ||
        e.altKey) {
      return;
    }
    e.preventDefault();
    if (e.repeat) {
      return;
    }
    this.refreshAll_();
  }

  private syncTabsWithUrlHash_() {
    const tabUrlHashes = [
      '#model-selection',
      '#debugging',
      '#eligibility',
      '#session-state',
    ];
    const tabBox = this.shadowRoot.querySelector('cr-tab-box')!;

    tabBox.addEventListener('selected-index-change', e => {
      const selectedHash = tabUrlHashes[e.detail] || '';
      window.location.hash = selectedHash;
      if (selectedHash === '#session-state') {
        this.$.sessionStateTab.refresh();
      }
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
      if (parsedUrl) {
        this.currentHost_ = parsedUrl.host;
        this.forcedHostError_ = '';
      } else {
        this.currentHost_ = '';
        this.forcedHostError_ = `Error: Invalid URL provided (${fullUrl})`;
      }
    } else {
      // Reset the current host if the forced host is empty.
      this.currentHost_ = '';
    }
  }

  protected getForcedParamEntries_(): Array<[string, string]> {
    return Object.entries(this.currentParams_)
        .sort(([a], [b]) => a.localeCompare(b));
  }

  protected getEffectiveHost_(): string {
    return this.currentHost_ || 'www.google.com';
  }

  protected getEffectiveHostClass_(): string {
    return this.currentHost_ ? 'url-host-override' : 'url-muted';
  }

  protected getForcedParamsSuffix_(): string {
    const entries = this.getForcedParamEntries_();
    if (entries.length === 0) {
      return '';
    }
    const searchParams = new URLSearchParams();
    for (const [key, value] of entries) {
      searchParams.set(key, value);
    }
    return `&${searchParams.toString()}`;
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
    this.forcedHostError_ = '';
  }

  protected onForcedHostKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.onSetForcedHostClick_();
    }
  }

  protected async onSetForcedHostClick_() {
    const rawInput = this.forcedHost_.trim();
    if (!rawInput) {
      await this.onResetForcedHostClick_();
      return;
    }

    const urlToParse =
        rawInput.includes('://') ? rawInput : `https://${rawInput}`;
    const url = URL.parse(urlToParse);

    // Check if the current URL is valid.
    if (!url) {
      this.forcedHostError_ =
          `Error: Invalid URL provided (${this.forcedHost_})`;
      return;
    }

    // Verify that the host is a Google domain.
    // LINT.IfChange(AllowedHosts)
    if (!url.hostname.endsWith('.google.com') &&
        !url.hostname.endsWith('.googlers.com') &&
        url.hostname !== 'google.com' && url.hostname !== 'googlers.com') {
      this.forcedHostError_ =
          `Error: URL must be a Google domain (.google.com or .googlers.com)`;
      return;
    }
    // LINT.ThenChange(//components/contextual_tasks/public/features.cc:AllowedHosts)

    if (url.port) {
      const portNum = Number(url.port);
      if (!Number.isInteger(portNum) || portNum < 1 || portNum > 65535) {
        this.forcedHostError_ = `Error: Invalid port (${url.port})`;
        return;
      }
    }

    this.forcedHostError_ = '';
    await this.proxy_.handler.setForcedEmbeddedPageHost(url.href);
    await this.refreshCurrentHost_();
  }

  protected async onResetForcedHostClick_() {
    this.forcedHost_ = '';
    this.forcedHostError_ = '';
    await this.proxy_.handler.setForcedEmbeddedPageHost('');
    await this.refreshCurrentHost_();
  }

  protected onParamKeyValueChanged_(e: CustomEvent<{value: string}>) {
    this.paramKey_ = e.detail.value;
    this.forcedParamsError_ = '';
  }

  protected onParamValueChanged_(e: CustomEvent<{value: string}>) {
    this.paramValue_ = e.detail.value;
    this.forcedParamsError_ = '';
  }

  protected onParamInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.onAddForcedParamClick_();
    }
  }

  protected async onAddForcedParamClick_() {
    const rawKey = this.paramKey_.trim();
    const rawValue = this.paramValue_.trim();

    if (!rawKey) {
      this.forcedParamsError_ = 'Error: Enter a parameter key.';
      return;
    }

    const nextParams: {[key: string]: string} = {
      ...this.currentParams_,
      [rawKey]: rawValue,
    };
    this.paramKey_ = '';
    this.paramValue_ = '';
    this.forcedParamsError_ = '';
    this.saveForcedParams_(nextParams);
    await this.updateComplete;
    this.$.paramKeyInput.focusInput();
  }

  protected async onEditForcedParamClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const key = target.dataset['key'];
    if (!key) {
      return;
    }
    this.paramKey_ = key;
    this.paramValue_ = target.dataset['value'] ?? '';
    this.forcedParamsError_ = '';
    await this.updateComplete;
    this.$.paramValueInput.select();
  }

  protected onRemoveForcedParamClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const key = target.dataset['key'];
    if (!key) {
      return;
    }
    const nextParams: {[key: string]: string} = {...this.currentParams_};
    delete nextParams[key];
    this.forcedParamsError_ = '';
    this.saveForcedParams_(nextParams);
  }

  protected async onResetAllOverridesClick_() {
    this.forcedHost_ = '';
    this.forcedHostError_ = '';
    this.paramKey_ = '';
    this.paramValue_ = '';
    this.forcedParamsError_ = '';
    this.saveForcedParams_({});
    await this.proxy_.handler.setForcedEmbeddedPageHost('');
    await this.refreshCurrentHost_();
  }

  private saveForcedParams_(params: {[key: string]: string}) {
    this.currentParams_ = params;
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
