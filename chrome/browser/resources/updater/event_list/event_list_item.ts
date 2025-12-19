// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './raw_event_details.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {UpdaterProcessMap} from '../event_history.js';
import {getAppId, isMergedHistoryEvent} from '../event_history.js';
import type {HistoryEvent, MergedActivateEvent, MergedAppCommandEvent, MergedHistoryEvent, MergedInstallEvent, MergedQualifyEvent, MergedUninstallEvent, MergedUpdateEvent, MergedUpdaterProcessEvent, PersistedDataEvent} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';
import {getKnownAppNamesById} from '../known_apps.js';

import {getCss} from './event_list_item.css.js';
import {getHtml} from './event_list_item.html.js';

// Prefix included in Omaha responses which prevent browsers from interpreting
// them as JavaScript. The content following the prefix is expected to be valid
// JSON.
const SAFE_JSON_PREFIX = ')]}\'';

/**
 * An item in the event list, representing a single event.
 */
export class EventListItemElement extends CrLitElement {
  static get is() {
    return 'event-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      event: {type: Object},
      eventDate: {type: Object},
      processMap: {type: Object},
      expanded: {type: Boolean, notify: true},
      error: {type: Boolean, reflect: true},
    };
  }

  accessor event: HistoryEvent|MergedHistoryEvent|undefined = undefined;
  accessor eventDate: Date|undefined = undefined;
  accessor processMap: UpdaterProcessMap|undefined = undefined;
  accessor expanded = false;
  accessor error = false;

  protected appId: string|undefined = undefined;
  protected appLabel: string|undefined = undefined;
  protected formattedDate: string|undefined = undefined;
  protected formattedDuration: string|undefined = undefined;
  protected errors: string[] = [];
  protected omahaRequest: Record<string, unknown>|undefined = undefined;
  protected omahaResponse: Record<string, unknown>|undefined = undefined;
  protected nextVersion: string|undefined = undefined;
  protected updaterVersion: string|undefined = undefined;
  protected commandLine: string|undefined = undefined;
  protected eventSummary: string|undefined = undefined;

  override willUpdate(changedProperties: PropertyValues<this>) {
    if (changedProperties.has('event') && this.event !== undefined) {
      this.appId = getAppId(this.event);
      this.appLabel = this.computeAppLabel();
      this.formattedDate = this.computeFormattedDate();
      this.formattedDuration = this.computeFormattedDuration();
      this.errors = this.computeErrors();
      this.error = this.errors.length > 0;
      this.omahaRequest = this.computeOmahaRequest();
      this.omahaResponse = this.computeOmahaResponse();
      this.nextVersion = this.computeNextVersion();
      this.commandLine = this.computeCommandLine();
      this.eventSummary = this.getEventSummary(this.event);
    }

    if (changedProperties.has('event') || changedProperties.has('processMap')) {
      this.updaterVersion = this.computeUpdaterVersion();
    }
  }

  expand() {
    this.expanded = true;
  }

  collapse() {
    this.expanded = false;
  }

  protected onExpandedChanged(e: CustomEvent<{value: boolean}>) {
    this.expanded = e.detail.value;
  }

  /**
   * Parses a base64-encoded string as JSON, discarding the `SAFE_JSON_PREFIX`
   * if present.
   */
  private base64ToJson(content: string): Record<string, unknown>|undefined {
    try {
      let decodedContent = atob(content);
      if (decodedContent.startsWith(SAFE_JSON_PREFIX)) {
        decodedContent = decodedContent.substring(SAFE_JSON_PREFIX.length);
      }
      return JSON.parse(decodedContent) as Record<string, unknown>;
    } catch (e) {
      return undefined;
    }
  }

  private computeAppLabel(): string {
    return this.appId !== undefined ?
        getKnownAppNamesById().get(this.appId.toUpperCase()) ?? this.appId :
        loadTimeData.getString('internal');
  }

  private computeFormattedDate(): string|undefined {
    return this.eventDate ? new Intl
                                .DateTimeFormat(undefined, {
                                  month: 'numeric',
                                  day: 'numeric',
                                  hour: 'numeric',
                                  minute: 'numeric',
                                  second: 'numeric',
                                })
                                .format(this.eventDate) :
                            undefined;
  }

  private computeFormattedDuration(): string|undefined {
    if (this.event === undefined || !isMergedHistoryEvent(this.event)) {
      return undefined;
    }
    let remainingUs =
        this.event.endEvent.deviceUptime - this.event.startEvent.deviceUptime;
    const microseconds = remainingUs % 1000;
    remainingUs = Math.floor(remainingUs / 1000);
    const milliseconds = remainingUs % 1000;
    remainingUs = Math.floor(remainingUs / 1000);
    const seconds = remainingUs % 60;
    remainingUs = Math.floor(remainingUs / 60);
    const minutes = remainingUs % 60;
    remainingUs = Math.floor(remainingUs / 60);
    const hours = remainingUs % 24;
    remainingUs = Math.floor(remainingUs / 24);
    const days = remainingUs;

    return new Intl
        .DurationFormat(undefined, {
          style: 'narrow',
        })
        .format({
          days,
          hours,
          minutes,
          seconds,
          milliseconds,
          microseconds,
        });
  }

  private computeErrors(): string[] {
    if (this.event === undefined) {
      return [];
    }
    const errors = isMergedHistoryEvent(this.event) ?
        [...this.event.startEvent.errors, ...this.event.endEvent.errors] :
        this.event.errors;
    return errors.map(
        error => loadTimeData.getStringF(
            'errorDetails', error.category, error.code, error.extracode1));
  }

  private computeOmahaRequest(): Record<string, unknown>|undefined {
    if (this.event === undefined || !isMergedHistoryEvent(this.event) ||
        this.event.eventType !== 'POST_REQUEST' ||
        this.event.startEvent.request.length === 0) {
      return undefined;
    }
    const json = this.base64ToJson(this.event.startEvent.request);
    return json && json['request'] ? json : undefined;
  }

  private computeOmahaResponse(): Record<string, unknown>|undefined {
    if (this.event === undefined || !isMergedHistoryEvent(this.event) ||
        this.event.eventType !== 'POST_REQUEST' ||
        this.event.endEvent.response === undefined ||
        this.event.endEvent.response.length === 0) {
      return undefined;
    }
    const json = this.base64ToJson(this.event.endEvent.response);
    return json !== undefined && json['response'] ? json : undefined;
  }

  private computeNextVersion(): string|undefined {
    if (this.event === undefined || !isMergedHistoryEvent(this.event) ||
        this.event.eventType !== 'UPDATE') {
      return undefined;
    }
    return this.event.endEvent.nextVersion;
  }

  private computeUpdaterVersion(): string|undefined {
    if (this.event === undefined || this.processMap === undefined) {
      return undefined;
    }
    return this.processMap.getUpdaterProcessForEvent(this.event)
        ?.startEvent.updaterVersion;
  }

  private computeCommandLine(): string|undefined {
    if (this.event === undefined || !isMergedHistoryEvent(this.event) ||
        (this.event.eventType !== 'UPDATER_PROCESS' &&
         this.event.eventType !== 'APP_COMMAND')) {
      return undefined;
    }
    return this.event.startEvent.commandLine;
  }

  private getInstallSummary(event: MergedInstallEvent): string|undefined {
    return event.endEvent.version !== undefined ?
        loadTimeData.getStringF('installSummary', event.endEvent.version) :
        undefined;
  }

  private getUninstallSummary(event: MergedUninstallEvent): string {
    return loadTimeData.getStringF(
        'uninstallSummary', event.startEvent.version, event.startEvent.reason);
  }

  private getQualifySummary(event: MergedQualifyEvent): string {
    return loadTimeData.getString(
        event.endEvent.qualified ? 'qualificationSucceeded' :
                                   'qualificationFailed');
  }

  private getActivateSummary(event: MergedActivateEvent): string {
    return loadTimeData.getString(
        event.endEvent.activated ? 'activationSucceeded' : 'activationFailed');
  }

  private getUpdateSummary(event: MergedUpdateEvent): string|undefined {
    switch (event.endEvent.outcome) {
      case 'NO_UPDATE':
        return loadTimeData.getString('noUpdate');
      case 'UPDATED':
        return event.endEvent.nextVersion ?
            loadTimeData.getStringF('updatedTo', event.endEvent.nextVersion) :
            undefined;
      case 'UPDATE_ERROR':
        return loadTimeData.getString('updateError');
      default:
        return event.endEvent.outcome ?
            loadTimeData.getStringF('outcome', event.endEvent.outcome) :
            loadTimeData.getString('outcomeUnknown');
    }
  }

  private getUpdaterProcessSummary(event: MergedUpdaterProcessEvent): string
      |undefined {
    if (event.startEvent.scope === undefined ||
        event.endEvent.exitCode === undefined) {
      return undefined;
    }
    return loadTimeData.getStringF('processSummary', event.endEvent.exitCode);
  }

  private getAppCommandSummary(event: MergedAppCommandEvent): string|undefined {
    return event.endEvent.exitCode !== undefined ?
        loadTimeData.getStringF('commandOutcome', event.endEvent.exitCode) :
        undefined;
  }

  private getPersistedDataSummary(event: PersistedDataEvent): string {
    return loadTimeData.getStringF(
        'persistedDataSummary',
        event.registeredApps.length,
    );
  }

  private getEventSummary(event: HistoryEvent|MergedHistoryEvent|undefined):
      string|undefined {
    if (event === undefined) {
      return undefined;
    }

    if (isMergedHistoryEvent(event)) {
      switch (event.eventType) {
        case 'INSTALL':
          return this.getInstallSummary(event);
        case 'UNINSTALL':
          return this.getUninstallSummary(event);
        case 'QUALIFY':
          return this.getQualifySummary(event);
        case 'ACTIVATE':
          return this.getActivateSummary(event);
        case 'UPDATE':
          return this.getUpdateSummary(event);
        case 'UPDATER_PROCESS':
          return this.getUpdaterProcessSummary(event);
        case 'APP_COMMAND':
          return this.getAppCommandSummary(event);
        default:
          return undefined;
      }
    } else {
      switch (event.eventType) {
        case 'PERSISTED_DATA':
          return this.getPersistedDataSummary(event);
        default:
          return undefined;
      }
    }
  }

  /**
   * Returns whether a chip should be added before the event description
   * indicating that the event is an HTTP POST likely containing an Omaha
   * request/response pair.
   */
  protected shouldShowOmahaRequestChip() {
    return this.event !== undefined && isMergedHistoryEvent(this.event) &&
        this.event.eventType === 'POST_REQUEST' &&
        (this.omahaRequest !== undefined || this.omahaResponse !== undefined);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'event-list-item': EventListItemElement;
  }
}

customElements.define(EventListItemElement.is, EventListItemElement);
