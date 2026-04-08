// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_list/app_list.js';
import './enterprise_policy_table/enterprise_policy_table.js';
import './event_list/event_list.js';
import './updater_state/updater_state.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AppStateDisplay} from './app_list/app_list.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {deduplicateEvents, isMergedHistoryEvent, mergeEvents, parseEvents, parsePolicySet, SCOPES, UpdaterProcessMap} from './event_history.js';
import type {MergedLoadPolicyEvent, PersistedDataEvent, PolicySet} from './event_history.js';
import {getKnownAppNamesById} from './known_apps.js';
import type {EnterpriseCompanionState, GetAppStatesResponse, GetEnterpriseCompanionStateResponse, GetUpdaterStatesResponse, UpdaterState} from './updater_ui.mojom-webui.js';

export enum PageDataSource {
  // The page displays current information about the updater installations on
  // the users system.
  INSTALL,
  // The page displays a snapshot of an updater installation given files it has
  // emitted (e.g. a updater_history.jsonl file or a chrome://support-tool zip).
  FILE,
}

export interface UpdaterAppElement {
  $: {
    fileInput: HTMLInputElement,
  };
}

export class UpdaterAppElement extends CrLitElement {
  static get is() {
    return 'updater-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      messages: {type: Array},
      userUpdaterState: {type: Object},
      systemUpdaterState: {type: Object},
      enterpriseCompanionState: {type: Object},
      updaterStateError: {type: Boolean},
      apps: {type: Array},
      appStateError: {type: Boolean},
      pageDataSource: {
        type: Number,
        reflect: true,
      },
      fileSelectionBannerLabel: {type: String},
      historyLoadError: {
        type: Boolean,
        reflect: true,
      },
      policies: {type: Object},
    };
  }

  accessor messages: Array<Record<string, unknown>> = [];
  accessor userUpdaterState: UpdaterState|null = null;
  accessor systemUpdaterState: UpdaterState|null = null;
  accessor enterpriseCompanionState: EnterpriseCompanionState|null = null;
  accessor updaterStateError = false;
  accessor apps: AppStateDisplay[] = [];
  accessor appStateError = false;
  accessor pageDataSource: PageDataSource = PageDataSource.INSTALL;
  accessor fileSelectionBannerLabel: string = '';
  accessor historyLoadError = false;

  protected accessor policies: PolicySet|undefined = undefined;


  override connectedCallback() {
    super.connectedCallback();
    if (this.pageDataSource === PageDataSource.INSTALL) {
      this.fetchInstallData();
    }
  }

  protected onLoadHistoryClick() {
    this.$.fileInput.click();
  }

  protected async onFileInputChange(e: Event) {
    const fileInput = e.target as HTMLInputElement;
    if (!fileInput.files || fileInput.files.length === 0) {
      return;
    }

    this.historyLoadError = false;
    const files = Array.from(fileInput.files);

    try {
      const records = await this.processHistoryFiles(files);
      const events = records.map(e => JSON.parse(e));
      this.fileSelectionBannerLabel =
          await PluralStringProxyImpl.getInstance().getPluralString(
              'viewingHistoryFiles', files.length);
      this.computeStateFromHistory(events);
      this.pageDataSource = PageDataSource.FILE;
    } catch (err) {
      console.error('Failed to load history file(s)', err);
      this.historyLoadError = true;
    } finally {
      fileInput.value = '';
    }
  }

  private async processHistoryFiles(files: File[]): Promise<string[]> {
    const processJSONL = (jsonl: string): string[] =>
        jsonl.trim()
            .split('\n')
            .map(line => line.trim())
            .filter(line => line.length > 0);
    const records = await Promise.all(files.map(async file => {
      if (/\.jsonl(\.old)?$/i.test(file.name)) {
        return processJSONL(await file.text());
      }
      if (file.name.toLowerCase().endsWith('.zip')) {
        const data = new Uint8Array(await file.arrayBuffer());
        const handle = Mojo.createSharedBuffer(data.byteLength).handle;
        const buffer =
            new Uint8Array(handle.mapBuffer(0, data.byteLength).buffer);
        buffer.set(data);

        const response = await BrowserProxyImpl.getInstance()
                             .handler.unzipUpdaterHistoryFiles({
                               sharedMemory: {
                                 bufferHandle: handle,
                                 size: data.byteLength,
                               },
                             });
        return response.historyFileContents.flatMap(processJSONL);
      }
      throw new Error(`No handler available for ${file.name}`);
    }));
    return records.flat();
  }

  protected onCloseFileClick() {
    this.pageDataSource = PageDataSource.INSTALL;
    this.fileSelectionBannerLabel = '';
    this.historyLoadError = false;
    this.fetchInstallData();
  }

  private fetchInstallData() {
    this.getAllUpdaterEvents().then(messages => this.messages = messages);
    this.getUpdaterStates()
        .then(response => {
          this.userUpdaterState = response.user;
          this.systemUpdaterState = response.system;
        })
        .catch(err => {
          this.updaterStateError = true;
          console.error('Failed to retrieve updater states', err);
        });
    this.getEnterpriseCompanionState()
        .then(response => {
          this.enterpriseCompanionState = response.state;
        })
        .catch((err) => {
          this.updaterStateError = true;
          console.error('Failed to retrieve enterprise companion state', err);
        });
    this.getAppStates().then(apps => this.apps = apps).catch((err) => {
      this.appStateError = true;
      console.error('Failed to retrieve application states', err);
    });
  }

  private computeStateFromHistory(rawMessages: Array<Record<string, unknown>>) {
    this.messages = rawMessages;
    const {valid} = parseEvents(rawMessages);
    const events = deduplicateEvents(valid);
    const {paired, unpaired} = mergeEvents(events);
    const processMap = new UpdaterProcessMap(paired);
    const {sortedEventsWithDates} =
        processMap.sortEventsByDate(unpaired, paired);

    const knownApps = getKnownAppNamesById();
    const apps: AppStateDisplay[] = [];
    let policies: PolicySet|undefined = undefined;
    for (const scope of SCOPES) {
      const latestPersistedData = sortedEventsWithDates.find(
          (e): e is PersistedDataEvent => e.eventType === 'PERSISTED_DATA' &&
              processMap.getUpdaterProcessForEvent(e)?.startEvent.scope ===
                  scope);
      if (latestPersistedData) {
        apps.push(...latestPersistedData.registeredApps.map(
            app => ({
              appId: app.appId,
              version: app.version,
              cohort: app.cohort || null,
              scope,
              displayName: knownApps.get(app.appId.toLowerCase()) || app.appId,
            })));
      }

      const latestLoadPolicy = sortedEventsWithDates.find(
          (e): e is MergedLoadPolicyEvent => e.eventType === 'LOAD_POLICY' &&
              isMergedHistoryEvent(e) &&
              processMap.getUpdaterProcessForEvent(e)?.startEvent.scope ===
                  scope);
      if (latestLoadPolicy) {
        policies = latestLoadPolicy.endEvent.policySet;
      }
    }

    this.apps = apps;
    this.enterpriseCompanionState = null;
    this.systemUpdaterState = null;
    this.userUpdaterState = null;
    this.updaterStateError = false;
    this.appStateError = false;
    this.policies = policies;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('systemUpdaterState') &&
        this.pageDataSource === PageDataSource.INSTALL) {
      this.policies = this.computePoliciesFromInstall();
    }
  }

  private computePoliciesFromInstall(): PolicySet|undefined {
    if (this.systemUpdaterState === null) {
      return undefined;
    }
    const policies = JSON.parse(this.systemUpdaterState.policies);
    try {
      return parsePolicySet({policies}, 'policies');
    } catch (e) {
      console.warn(`Failed to parse policy set: ${e}. Message: ${policies}`);
      return undefined;
    }
  }

  private async getAllUpdaterEvents(): Promise<Array<Record<string, unknown>>> {
    const response =
        await BrowserProxyImpl.getInstance().handler.getAllUpdaterEvents();

    return response.events.map(message => JSON.parse(message))
        .filter(message => typeof message === 'object');
  }

  private async getUpdaterStates(): Promise<GetUpdaterStatesResponse> {
    return await BrowserProxyImpl.getInstance().handler.getUpdaterStates();
  }

  private async getEnterpriseCompanionState():
      Promise<GetEnterpriseCompanionStateResponse> {
    return await BrowserProxyImpl.getInstance()
        .handler.getEnterpriseCompanionState();
  }

  private async getAppStates(): Promise<AppStateDisplay[]> {
    const response: GetAppStatesResponse =
        await BrowserProxyImpl.getInstance().handler.getAppStates();
    const knownApps = getKnownAppNamesById();

    const systemApps: AppStateDisplay[] = response.systemApps.map(
        app => ({
          ...app,
          scope: 'SYSTEM',
          displayName: knownApps.get(app.appId.toLowerCase()) || app.appId,
        }));
    const userApps: AppStateDisplay[] = response.userApps.map(
        app => ({
          ...app,
          scope: 'USER',
          displayName: knownApps.get(app.appId.toLowerCase()) || app.appId,
        }));
    return [...systemApps, ...userApps];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'updater-app': UpdaterAppElement;
  }
}

customElements.define(UpdaterAppElement.is, UpdaterAppElement);
