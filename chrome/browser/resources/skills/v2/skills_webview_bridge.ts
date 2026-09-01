// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {Skill} from '../skill.mojom-webui.js';
import type {PendingEditorData} from '../skills.mojom-webui.js';

import {getLoadingStageHistogramName, getPrimarySkillsOrigin, getSkillsApiAllowedOrigins, HANDSHAKE_PING_INTERVAL_MS, HANDSHAKE_TIMEOUT_MS, HISTOGRAM_HANDSHAKE_RESULT, HISTOGRAM_WRITE_LATENCY, LoadingStage, SKILLS_CLOSE_DIALOG, SKILLS_DIALOG_INFO_TYPE, SKILLS_GEMINI_PROMPT_TYPE, SKILLS_GET_PROVIDED_SKILL, SKILLS_HANDSHAKE_ACK, SKILLS_HANDSHAKE_TYPE, SKILLS_INVOKE_SKILL, SKILLS_LOG_METRIC, SKILLS_LOG_UMA_ENUM, SKILLS_OPEN_FULL_PAGE_EDITOR, SKILLS_OPEN_URL, SKILLS_PROVIDED_SKILL_INFO_TYPE, SKILLS_SEND_PROMPT, SKILLS_SEND_PROVIDED_SKILLS_TYPE, SKILLS_SHOW_TOAST, SKILLS_TOAST_CLOSED_TYPE, SKILLS_UNDO_TYPE, SKILLS_UPDATED_TYPE} from './skills_webview_bridge_constants.js';

export interface SkillPreview {
  id: string;
  name: string;
  icon: string|undefined;
  imageUrl: string|undefined;
  description: string|undefined;
  category: string|undefined;
}
/**
 * Returns a URLPattern given an origin pattern string that has the syntax:
 * <protocol>://<hostname>[:<port>]
 * where <protocol>, <hostname> and <port> are inserted into URLPattern.
 */
export function matcherForOrigin(originPattern: string): URLPattern|null {
  const match = originPattern.match(/([^:]+):\/\/([^:]*)(?::(\d+))?[/]?/);
  if (!match) {
    return null;
  }

  const [protocol, hostname, port] = [match[1], match[2], match[3] ?? '*'];
  try {
    return new URLPattern({protocol, hostname, port});
  } catch (_) {
    return null;
  }
}

function isInternalOnlyOrigin(origin: string): boolean {
  return origin === 'https://login.corp.google.com' ||
      origin === 'https://accounts.googlers.com' ||
      origin === 'https://gaiastaging.corp.google.com' ||
      origin.endsWith('.proxy.googlers.com');
}

export function urlMatchesApiAllowedOrigin(url: URL): boolean {
  if (url.origin === 'null') {
    return false;
  }

  // For development and testing.
  if (loadTimeData.getBoolean('devMode')) {
    return true;
  }

  // A URL is allowed to have API access if it matches any of the explicit API
  // allowed origins.
  return getSkillsApiAllowedOrigins().some((origin: string) => {
    // Only allow internal origins for internal users.
    if (isInternalOnlyOrigin(origin) &&
        !loadTimeData.getBoolean('isInternalUser')) {
      return false;
    }
    return matcherForOrigin(origin.trim())?.test(url);
  });
}

// TODO(b/529400161): Consider moving to another file.
export interface SkillsWebviewBridgeDelegate {
  onError(): void;
  onShowSaveToast(): void;
  onShowSaveAndInvokeToast(
      skillId: string, skillName: string, skillIcon: string): void;
  onShowDeleteToast(skillId: string): Promise<{actionClicked: boolean}>;
  onInvokeSkill(skillId: string, skillName: string, skillIcon: string): void;
  onUrlChanged(url: URL): void;
  onCloseDialog(): void;
  onCloseDialogAndOpenEditor(data: PendingEditorData): void;
  onHandshakeStarted(): void;
  onHandshakeComplete(): void;
  onSendPrompt(prompt: string): void;
  onGetProvidedSkill(skillId: string): void;
}

/**
 * A bridge class that manages the postMessage handshake and communication
 * between the Chrome WebUI host and the guest Webview application.
 */
export class SkillsWebviewBridge {
  private webview_: chrome.webviewTag.WebView;
  private targetOrigin_: string = '';
  private handshakeIntervalId_: number|null = null;
  private timeoutId_: number|null = null;
  private isConnected_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();
  private delegate_: SkillsWebviewBridgeDelegate;
  private handshakeStartTime_: number|null = null;
  private isInitialHandshake_: boolean = true;
  private isInitialGuestFramework_: boolean = true;
  private isInitialGuestWebClient_: boolean = true;
  private isInitialGuestDataFetch_: boolean = true;

  constructor(
      webview: chrome.webviewTag.WebView,
      delegate: SkillsWebviewBridgeDelegate) {
    assert(loadTimeData.getBoolean('isSkillsWebViewV2Enabled'));
    this.webview_ = webview;
    this.delegate_ = delegate;

    this.eventTracker_.add(
        this.webview_, 'loadcommit',
        (e: Event) =>
            this.onLoadCommit(e as chrome.webviewTag.LoadCommitEvent));
    this.eventTracker_.add(this.webview_, 'loadstop', () => this.onLoadStop());
    this.eventTracker_.add(
        window, 'message', (e: MessageEvent) => this.onMessage(e));
  }

  private onLoadCommit(e: chrome.webviewTag.LoadCommitEvent) {
    if (!e.isTopLevel) {
      return;
    }

    const urlObj = URL.parse(e.url);

    // Disallowed Origin.
    if (!urlObj || !urlMatchesApiAllowedOrigin(urlObj)) {
      this.delegate_.onError();
      return;
    }

    this.delegate_.onUrlChanged(urlObj);

    // Start handshake if valid target url.
    if (this.urlRequiresHandshake(urlObj)) {
      this.targetOrigin_ = urlObj.origin;
      this.webview_.setAttribute('hidden', 'true');
      this.startHandshake();
    }
  }

  private urlRequiresHandshake(url: URL): boolean {
    // If we are already connected we don't need a new handshake.
    if (this.isConnected_) {
      return false;
    }
    // For development and testing.
    if (loadTimeData.getBoolean('devMode')) {
      return true;
    }

    return matcherForOrigin(getPrimarySkillsOrigin())?.test(url) ?? false;
  }

  private onLoadStop() {
    if (this.webview_.checkVisibility?.()) {
      this.webview_.focus();
    }
  }

  private startHandshake() {
    // Reset in case of successive handshakes.
    this.isConnected_ = false;
    this.stopHandshake();
    this.handshakeStartTime_ = performance.now();
    this.delegate_.onHandshakeStarted();

    // Send a handshake ping periodically.
    this.handshakeIntervalId_ = window.setInterval(() => {
      this.sendPing();
    }, HANDSHAKE_PING_INTERVAL_MS);

    // Set a timeout to abort handshake.
    this.timeoutId_ = window.setTimeout(() => {
      this.recordHandshakeFailureMetric();
      this.stopHandshake();
      this.delegate_.onError();
    }, HANDSHAKE_TIMEOUT_MS);

    this.sendPing();
  }

  private sendPing() {
    if (!this.targetOrigin_) {
      return;
    }
    if (this.webview_.contentWindow) {
      this.webview_.contentWindow.postMessage(
          {type: SKILLS_HANDSHAKE_TYPE}, this.targetOrigin_);
    }
  }

  private stopHandshake() {
    if (this.handshakeIntervalId_ !== null) {
      window.clearInterval(this.handshakeIntervalId_);
      this.handshakeIntervalId_ = null;
    }
    if (this.timeoutId_ !== null) {
      window.clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }
  }

  private onMessage(e: MessageEvent) {
    if (this.targetOrigin_ && e.origin !== this.targetOrigin_) {
      return;
    }
    if (!e.data) {
      return;
    }

    // Handle handshake ack if guest replies.
    if (e.data.type === SKILLS_HANDSHAKE_ACK) {
      this.isConnected_ = true;
      this.webview_.removeAttribute('hidden');
      this.stopHandshake();
      if (this.isInitialHandshake_) {
        this.recordInitialHandshakeMetrics();
        this.isInitialHandshake_ = false;
      }
      this.delegate_.onHandshakeComplete();
    }

    // Before we process non-handshake message, make sure we are connected.
    if (!this.isConnected_) {
      return;
    }

    if (e.data.type === SKILLS_SHOW_TOAST) {
      this.handleShowToastMessage(e.data);
    } else if (e.data.type === SKILLS_INVOKE_SKILL) {
      this.handleInvokeSkillMessage(e.data);
    } else if (e.data.type === SKILLS_CLOSE_DIALOG) {
      this.delegate_.onCloseDialog();
    } else if (e.data.type === SKILLS_LOG_METRIC) {
      this.handleLogMetricMessage(e.data);
    } else if (e.data.type === SKILLS_LOG_UMA_ENUM) {
      this.handleLogUmaEnumMessage(e.data);
    } else if (e.data.type === SKILLS_OPEN_URL) {
      this.handleOpenUrlMessage(e.data);
    } else if (e.data.type === SKILLS_SEND_PROMPT) {
      this.handleSendPromptMessage(e.data);
    } else if (e.data.type === SKILLS_OPEN_FULL_PAGE_EDITOR) {
      this.handleOpenFullPageEditorMessage(e.data);
    } else if (e.data.type === SKILLS_GET_PROVIDED_SKILL) {
      this.handleGetProvidedSkillMessage(e.data);
    }
  }

  private recordHandshakeFailureMetric() {
    if (this.isInitialHandshake_) {
      chrome.histograms.recordBoolean(HISTOGRAM_HANDSHAKE_RESULT, false);
      this.isInitialHandshake_ = false;
    }
  }

  private recordInitialHandshakeMetrics() {
    if (this.handshakeStartTime_ !== null) {
      const handshakeDuration = performance.now() - this.handshakeStartTime_;
      chrome.histograms.recordMediumTime(
          getLoadingStageHistogramName(LoadingStage.HANDSHAKE),
          Math.floor(handshakeDuration));
    }
    chrome.histograms.recordBoolean(HISTOGRAM_HANDSHAKE_RESULT, true);
  }

  private async handleShowToastMessage(data: {
    toastType: string,
    skillId?: string,
    skillName?: string,
    skillIcon?: string,
  }) {
    // TODO(b/529405584): Refactor toastType to be an enum & consider how we
    // want to surface errors to the user if skillId does not exist.
    if (data.toastType === 'save') {
      this.delegate_.onShowSaveToast();
    } else if (data.toastType === 'delete') {
      const skillId = data.skillId ?? '';
      const response = await this.delegate_.onShowDeleteToast(skillId);
      if (response.actionClicked) {
        this.sendUndo(skillId);
      } else {
        this.sendToastClosed(skillId);
      }
    } else if (data.toastType === 'save_and_invoke') {
      this.delegate_.onShowSaveAndInvokeToast(
          data.skillId ?? '', data.skillName ?? '', data.skillIcon ?? '');
    }
  }

  private sendUndo(skillId: string) {
    if (this.webview_.contentWindow && this.targetOrigin_) {
      this.webview_.contentWindow.postMessage(
          {
            'type': SKILLS_UNDO_TYPE,
            'skillId': skillId,
          },
          this.targetOrigin_);
    }
  }

  private sendToastClosed(skillId: string) {
    if (this.webview_.contentWindow && this.targetOrigin_) {
      this.webview_.contentWindow.postMessage(
          {
            'type': SKILLS_TOAST_CLOSED_TYPE,
            'skillId': skillId,
          },
          this.targetOrigin_);
    }
  }

  private handleInvokeSkillMessage(data: {
    skillId: string,
    skillName: string,
    skillIcon: string,
  }) {
    if (data.skillId) {
      this.delegate_.onInvokeSkill(
          data.skillId, data.skillName, data.skillIcon);
    }
  }
  private handleLogMetricMessage(data: {metricName: string, valueMs: number}) {
    const valueMs = Math.floor(data.valueMs);
    if (data.metricName === 'framework-load-time' &&
        this.isInitialGuestFramework_) {
      chrome.histograms.recordMediumTime(
          getLoadingStageHistogramName(LoadingStage.GUEST_FRAMEWORK), valueMs);
      this.isInitialGuestFramework_ = false;
    } else if (
        data.metricName === 'web-client-load-time' &&
        this.isInitialGuestWebClient_) {
      chrome.histograms.recordMediumTime(
          getLoadingStageHistogramName(LoadingStage.GUEST_WEB_CLIENT), valueMs);
      this.isInitialGuestWebClient_ = false;
    } else if (
        data.metricName === 'guest-data-fetch-time' &&
        this.isInitialGuestDataFetch_) {
      chrome.histograms.recordMediumTime(
          getLoadingStageHistogramName(LoadingStage.GUEST_DATA_FETCH), valueMs);
      this.isInitialGuestDataFetch_ = false;
    } else if (data.metricName === 'guest-data-save-time') {
      chrome.histograms.recordMediumTime(HISTOGRAM_WRITE_LATENCY, valueMs);
    }
  }

  private handleLogUmaEnumMessage(data: {
    histogramName: string,
    value: number,
    enumSize: number,
  }) {
    if (typeof data.histogramName !== 'string' ||
        !data.histogramName.startsWith('Skills.') ||
        !Number.isInteger(data.value) || !Number.isInteger(data.enumSize) ||
        data.value < 0 || data.value >= data.enumSize || data.enumSize <= 0) {
      console.warn('Invalid UMA enum log payload:', data);
      return;
    }
    chrome.histograms.recordEnumerationValue(
        data.histogramName, data.value, data.enumSize);
  }

  private handleOpenUrlMessage(data: {url: string}) {
    window.open(data.url, '_blank');
  }

  private handleSendPromptMessage(data: {prompt: string}) {
    if (data.prompt) {
      this.delegate_.onSendPrompt(data.prompt);
    }
  }

  private handleOpenFullPageEditorMessage(data: {
    url: string,
    skillIcon: string,
    skillName: string,
    skillDescription: string,
    skillInstructions: string,
  }) {
    const urlObj = new URL(data.url, getPrimarySkillsOrigin());

    // Close the dialog and open the editor page.
    this.delegate_.onCloseDialogAndOpenEditor({
      name: data.skillName ?? '',
      description: data.skillDescription ?? '',
      instructions: data.skillInstructions ?? '',
      icon: data.skillIcon ?? '',
      url: urlObj.toString(),
    });
  }

  sendSkillDialogInfo(info: {
    skillIcon?: string,
    skillName?: string,
    skillDescription?: string,
    skillInstructions?: string,
  }) {
    if (this.webview_.contentWindow && this.targetOrigin_) {
      this.webview_.contentWindow.postMessage(
          {
            type: SKILLS_DIALOG_INFO_TYPE,
            skillIcon: info.skillIcon,
            skillName: info.skillName,
            skillDescription: info.skillDescription,
            skillInstructions: info.skillInstructions,
          },
          this.targetOrigin_);
    }
  }

  private handleGetProvidedSkillMessage(data: {skillId: string}) {
    if (data.skillId) {
      this.delegate_.onGetProvidedSkill(data.skillId);
    }
  }

  sendGeminiPrompt(prompt: string) {
    if (this.webview_.contentWindow && this.targetOrigin_) {
      this.webview_.contentWindow.postMessage(
          {
            type: SKILLS_GEMINI_PROMPT_TYPE,
            prompt: prompt,
          },
          this.targetOrigin_);
    }
  }

  sendProvidedSkills(skills: Skill[]) {
    if (this.webview_.contentWindow && this.targetOrigin_) {
      const payload: SkillPreview[] =
          skills.map(skill => ({
                       id: skill.id,
                       name: skill.name,
                       icon: skill.icon,
                       imageUrl: skill.imageUrl ? skill.imageUrl : undefined,
                       description: skill.description,
                       category: skill.category ? skill.category : undefined,
                     }));

      this.webview_.contentWindow.postMessage(
          {
            type: SKILLS_SEND_PROVIDED_SKILLS_TYPE,
            payload: payload,
          },
          this.targetOrigin_);
    }
  }

  sendProvidedSkillInfo(skill: Skill|null) {
    if (this.webview_.contentWindow && this.targetOrigin_) {
      this.webview_.contentWindow.postMessage(
          {
            type: SKILLS_PROVIDED_SKILL_INFO_TYPE,
            payload: skill ?? null,
          },
          this.targetOrigin_);
    }
  }

  sendSkillsUpdated() {
    if (this.webview_.contentWindow && this.targetOrigin_) {
      this.webview_.contentWindow.postMessage(
          {
            type: SKILLS_UPDATED_TYPE,
          },
          this.targetOrigin_);
    }
  }

  destroy() {
    this.stopHandshake();
    this.eventTracker_.removeAll();
  }

  isConnected(): boolean {
    return this.isConnected_;
  }
}
