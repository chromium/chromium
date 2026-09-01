// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../loading_page.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from '//resources/js/util.js';

import {ErrorType} from '../error_page.js';
import type {Skill} from '../skill.mojom-webui.js';
import {SkillsDialogType} from '../skill.mojom-webui.js';
import type {PendingEditorData, SkillsPageV2Interface} from '../skills.mojom-webui.js';
import {SkillsPageHandler, SkillsPageV2Receiver} from '../skills.mojom-webui.js';

import type {SkillsWebviewBridgeDelegate} from './skills_webview_bridge.js';
import {SkillsWebviewBridge} from './skills_webview_bridge.js';
import {getChromePathForRemoteUrl, getLoadingStageHistogramName, getRemoteUrlForChromePath, HISTOGRAM_TOTAL_INIT_LATENCY, IS_SAVING_GEMINI_QUERY_PARAMETER, LoadingStage, SkillSource, SOURCE_QUERY_PARAMETER} from './skills_webview_bridge_constants.js';

export class SkillsWebview implements SkillsPageV2Interface {
  protected remoteUrl: string = '';
  protected handler = SkillsPageHandler.getRemote();
  protected pageReceiver_: SkillsPageV2Receiver =
      new SkillsPageV2Receiver(this);
  protected webview: chrome.webviewTag.WebView|null = null;
  protected bridge: SkillsWebviewBridge|null = null;
  private promptToSend = '';
  private initStartTime_: number = 0;
  private navigationStartTime_: number = 0;
  private isInitialNavigation_: boolean = false;
  private hasLoggedInitLatency_: boolean = false;

  constructor() {
    this.initializeRemoteUrl();
  }

  private isSavingGeminiQuery(): boolean {
    return loadTimeData.getInteger('dialogType') === SkillsDialogType.kAdd &&
        !loadTimeData.getString('skillId') &&
        !!loadTimeData.getString('skillPrompt');
  }

  private isFirstPartySkill(): boolean {
    return loadTimeData.getInteger('dialogType') === SkillsDialogType.kAdd &&
        !!loadTimeData.getString('skillId');
  }

  private isUserSkill(): boolean {
    return loadTimeData.getInteger('dialogType') === SkillsDialogType.kEdit;
  }

  private initializeRemoteUrl() {
    const path = window.location.pathname;
    this.remoteUrl = getRemoteUrlForChromePath(path);

    const url = new URL(this.remoteUrl);

    // For dialog type urls, set query parameters as necessary.
    if (loadTimeData.valueExists('dialogType')) {
      if (this.isSavingGeminiQuery()) {
        url.searchParams.set(IS_SAVING_GEMINI_QUERY_PARAMETER, 'true');
        this.promptToSend = loadTimeData.getString('skillPrompt');
      } else if (this.isFirstPartySkill()) {
        url.searchParams.set('id', loadTimeData.getString('skillId'));
        url.searchParams.set(SOURCE_QUERY_PARAMETER, SkillSource.FIRST_PARTY);
      } else if (this.isUserSkill()) {
        const skillId = loadTimeData.getString('skillId');
        if (skillId) {
          url.searchParams.set('id', skillId);
          url.searchParams.set(SOURCE_QUERY_PARAMETER, SkillSource.USER);
        }
      }
    }

    this.remoteUrl = url.toString();
  }

  getInitStartTimeForTesting(searchParams: URLSearchParams): number {
    const openStartTime = searchParams.get('openStartTime');
    if (openStartTime) {
      return parseFloat(openStartTime) - performance.timeOrigin;
    }
    return performance.now();
  }

  async init() {
    this.initStartTime_ = this.getInitStartTimeForTesting(
        new URLSearchParams(window.location.search));
    if (window.location.search) {
      window.history.replaceState({}, '', window.location.pathname);
    }
    this.webview = getRequiredElement<chrome.webviewTag.WebView>('webview');

    if (loadTimeData.valueExists('isSkillsEnabled') &&
        !loadTimeData.getBoolean('isSkillsEnabled')) {
      this.showError(ErrorType.SKILLS_DISABLED);
      return;
    }

    this.handler.setPage(this.pageReceiver_.$.bindNewPipeAndPassRemote());

    // Wait for cookie sync to complete before setting src
    const success = await this.syncCookiesAndRecordMetric();

    if (!success) {
      this.showError(ErrorType.GLIC_NOT_ENABLED);
      return;
    }

    // If we are opening an editor page, check if we have pending prompt data to
    // send.
    let pendingData: PendingEditorData|null = null;
    if (window.location.pathname === '/editor') {
      const {data} = await this.handler.getPendingEditorData();
      pendingData = data;
      if (pendingData) {
        this.remoteUrl = pendingData.url;
      }
    }

    const {skills} = await this.handler.getProvidedSkills();

    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => this.showError(ErrorType.REMOTE_AUTHORITY_UNREACHABLE),
      onShowSaveToast: () => this.handler.showSaveToast(),
      onShowSaveAndInvokeToast: (
          skillId: string, skillName: string, skillIcon: string) =>
          this.handler.showSaveAndInvokeToast(skillId, skillName, skillIcon),
      onShowDeleteToast: (skillId: string) =>
          this.handler.showDeleteToast(skillId),
      onInvokeSkill: (skillId: string, skillName: string, skillIcon: string) =>
          this.handler.invokeSkill(skillId, skillName, skillIcon),
      onUrlChanged: (url: URL) => this.handleUrlChanged(url),
      onCloseDialog: () => this.handler.closeDialog(null),
      onHandshakeStarted: () => this.showLoading(),
      onHandshakeComplete: () => {
        this.hideLoading();
        this.recordTotalInitLatencyMetric();
      },
      onSendPrompt: (prompt: string) => this.handler.sendPrompt(prompt),
      onCloseDialogAndOpenEditor: (data: PendingEditorData) =>
          this.handler.closeDialog(data),
      onGetProvidedSkill: async (skillId: string) => {
        const {skill} = await this.handler.getProvidedSkill(skillId);
        if (this.bridge) {
          this.bridge.sendProvidedSkillInfo(skill);
        }
      },
    };

    // Initiate handshake. Show error page on failure.
    this.bridge = new SkillsWebviewBridge(this.webview, delegate);
    this.navigationStartTime_ = performance.now();
    this.isInitialNavigation_ = true;
    this.webview.setAttribute('src', this.remoteUrl);

    // Send remote url more information, if needed.
    this.webview.addEventListener('loadstop', () => {
      if (this.promptToSend) {
        this.bridge?.sendGeminiPrompt(this.promptToSend);
        this.promptToSend = '';
      }
      if (pendingData) {
        this.bridge?.sendSkillDialogInfo({
          skillIcon: pendingData.icon,
          skillName: pendingData.name,
          skillDescription: pendingData.description,
          skillInstructions: pendingData.instructions,
        });
      }
      if (skills) {
        this.loadProvidedSkills(skills);
      }
    });

    // Manually handle clicks on forward and back buttons.
    window.addEventListener('popstate', () => this.syncWebviewToPath());
  }

  private setEditorStatus(path: string = window.location.pathname) {
    const loadingPage = document.querySelector('loading-page');
    if (loadingPage && !loadingPage.dialog) {
      loadingPage.editor = path === '/editor';
    }
  }

  private syncWebviewToPath() {
    this.setEditorStatus();
    const remoteUrl = getRemoteUrlForChromePath(window.location.pathname);
    if (this.webview && this.webview.getAttribute('src') !== remoteUrl) {
      this.webview.setAttribute('src', remoteUrl);
    }
  }

  protected handleUrlChanged(url: URL) {
    this.recordInitialNavigationMetric();

    const chromePath = getChromePathForRemoteUrl(url);
    this.setEditorStatus(chromePath);
    if (window.location.pathname === chromePath) {
      return;
    }
    // Updates url in address bar and adds it to history list.
    window.history.pushState({}, '', chromePath);
  }

  protected showError(errorType: ErrorType) {
    this.hideLoading();
    const errorPage = document.querySelector('error-page');
    if (errorPage) {
      errorPage.errorType = errorType;
      errorPage.removeAttribute('hidden');
    }
    this.webview?.setAttribute('hidden', 'true');
  }

  protected showLoading() {
    this.setEditorStatus();
    const loadingPage = document.querySelector('loading-page');
    if (loadingPage) {
      loadingPage.removeAttribute('hidden');
    }
  }

  protected hideLoading() {
    const loadingPage = document.querySelector('loading-page');
    if (loadingPage) {
      loadingPage.setAttribute('hidden', 'true');
    }
  }

  private async syncCookiesAndRecordMetric(): Promise<boolean> {
    const syncStart = performance.now();
    const {success} = await this.handler.syncCookies();
    const syncDuration = performance.now() - syncStart;
    chrome.histograms.recordMediumTime(
        getLoadingStageHistogramName(LoadingStage.COOKIE_SYNC),
        Math.floor(syncDuration));
    return success;
  }

  private recordTotalInitLatencyMetric() {
    if (!this.hasLoggedInitLatency_) {
      const totalDuration = performance.now() - this.initStartTime_;
      chrome.histograms.recordMediumTime(
          HISTOGRAM_TOTAL_INIT_LATENCY, Math.floor(totalDuration));
      this.hasLoggedInitLatency_ = true;
    }
  }

  private recordInitialNavigationMetric() {
    if (this.isInitialNavigation_) {
      const navDuration = performance.now() - this.navigationStartTime_;
      chrome.histograms.recordMediumTime(
          getLoadingStageHistogramName(LoadingStage.NAVIGATION),
          Math.floor(navDuration));
      this.isInitialNavigation_ = false;
    }
  }

  loadProvidedSkills(skills: Skill[]) {
    this.bridge?.sendProvidedSkills(skills);
  }

  onUserSkillsUpdated() {
    this.bridge?.sendSkillsUpdated();
  }
}
