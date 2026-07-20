// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from '//resources/js/util.js';

import {ErrorType} from '../error_page.js';
import {SkillsPageHandler} from '../skills.mojom-webui.js';
import type {ToastType} from '../skills.mojom-webui.js';

import type {SkillsWebviewBridgeDelegate} from './skills_webview_bridge.js';
import {SkillsWebviewBridge} from './skills_webview_bridge.js';
import {getChromePathForRemoteUrl, getRemoteUrlForChromePath} from './skills_webview_bridge_constants.js';

export class SkillsWebview {
  protected remoteUrl: string;
  protected handler = SkillsPageHandler.getRemote();
  protected webview: chrome.webviewTag.WebView|null = null;
  protected bridge: SkillsWebviewBridge|null = null;

  constructor() {
    // Resolve the staging remote URL dynamically based on the current Chrome
    // WebUI path.
    this.remoteUrl = getRemoteUrlForChromePath(window.location.pathname);
  }

  async init() {
    this.webview = getRequiredElement<chrome.webviewTag.WebView>('webview');

    // Wait for cookie sync to complete before setting src
    const {success} = await this.handler.syncCookies();
    if (!success) {
      this.showError(ErrorType.GLIC_NOT_ENABLED);
      return;
    }

    const delegate: SkillsWebviewBridgeDelegate = {
      onError: () => this.showError(ErrorType.REMOTE_AUTHORITY_UNREACHABLE),
      onShowToast: (toastType: ToastType) => this.handler.showToast(toastType),
      onInvokeSkill: (skillId: string) => this.handler.invokeSkill(skillId),
      onUrlChanged: (url: URL) => this.handleUrlChanged(url),
    };

    // Initiate handshake. Show error page on failure.
    this.bridge = new SkillsWebviewBridge(this.webview, delegate);
    this.webview.setAttribute('src', this.remoteUrl);

    // Manually handle clicks on forward and back buttons.
    window.addEventListener('popstate', () => this.syncWebviewToPath());
  }

  private syncWebviewToPath() {
    const remoteUrl = getRemoteUrlForChromePath(window.location.pathname);
    if (this.webview && this.webview.getAttribute('src') !== remoteUrl) {
      this.webview.setAttribute('src', remoteUrl);
    }
  }

  protected handleUrlChanged(url: URL) {
    const chromePath = getChromePathForRemoteUrl(url);
    if (window.location.pathname === chromePath) {
      return;
    }
    // Updates url in address bar and adds it to history list.
    window.history.pushState({}, '', chromePath);
  }

  protected showError(errorType: ErrorType) {
    const errorPage = document.querySelector('error-page');
    if (errorPage) {
      errorPage.errorType = errorType;
      errorPage.removeAttribute('hidden');
    }
    this.webview?.setAttribute('hidden', 'true');
  }
}
