// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../contextual_tasks_internals.mojom-webui.js';
import type {BrowserProxy, TabContextEntryDebug, WebContentsDebugState as MojomWebContentsDebugState} from '../contextual_tasks_internals.mojom-webui.js';

import {getCss} from './session_state_tab.css.js';
import {getHtml} from './session_state_tab.html.js';

export interface DeselectedTabDebug {
  tabId: number;
  title: string;
  url: string;
}

export interface FileInfoDebug {
  fileToken: string;
  fileName: string;
  fileSizeBytes: bigint;
  mimeType: string;
  uploadStatus: string;
  uploadErrorType: string;
  requestId: string;
  tabSessionId: number;
  tabUrl: string;
  tabTitle: string;
  isSuperceded: boolean;
  isImplicitUpload: boolean;
}

export interface UrlResourceDebug {
  urlId: string;
  url: string;
  tabId: number;
  title: string;
  contextId: bigint;
  resourceType: string;
}

export interface SessionHandleDebugState {
  sessionId: string;
  searchSessionId: string;
  currentUrl: string;
  invocationSource: string;
  isContextualLensSession: boolean;
  authUserIndex: number;
  hasSubmittedContext: boolean;
  smartTabSharingActive: boolean;
  smartTabSharingToggledSinceLastTurn: boolean;
  stsToggledRemovedContextIds: string[];
  attachedTabs: TabContextEntryDebug[];
  restoredTabs: TabContextEntryDebug[];
  deselectedTabs: DeselectedTabDebug[];
  uploadedContextTokens: string[];
  submittedContextTokens: string[];
  activeSubmittedContextTokens: string[];
  persistedTabIds: number[];
  pendingDelayedTabIds: number[];
  uploadedFiles: FileInfoDebug[];
  submittedFiles: FileInfoDebug[];
  previousTurns: string[];
  threadType: string;
  threadServerId: string;
  threadTitle: string;
  threadConversationTurnId: string;
  urlResources: UrlResourceDebug[];
}

export interface WebContentsDebugState extends MojomWebContentsDebugState {
  sessionHandleState?: SessionHandleDebugState|null;
}

export class ContextualTasksSessionStateTabElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-session-state-tab';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      webContentsDebugStates_: {type: Array},
    };
  }

  protected accessor webContentsDebugStates_: WebContentsDebugState[] = [];

  private proxy_: BrowserProxy = browserProxyFactory.getInstance();

  async refresh() {
    const response = await this.proxy_.handler.getSessionAndUserDataState();
    this.webContentsDebugStates_ = response.webContentsStates;
  }

  protected async onRefreshSessionStateClick_() {
    await this.refresh();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-session-state-tab': ContextualTasksSessionStateTabElement;
  }
}

customElements.define(
    ContextualTasksSessionStateTabElement.is,
    ContextualTasksSessionStateTabElement);
