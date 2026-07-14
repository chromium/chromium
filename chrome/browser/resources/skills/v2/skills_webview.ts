// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from '//resources/js/util.js';

import {ErrorType} from '../error_page.js';
import {SkillsPageHandler} from '../skills.mojom-webui.js';
import type {ToastType} from '../skills.mojom-webui.js';

import type {SkillsWebviewBridgeDelegate} from './skills_webview_bridge.js';
import {SkillsWebviewBridge} from './skills_webview_bridge.js';

export class SkillsWebview {
  protected url: string;
  protected handler = SkillsPageHandler.getRemote();
  protected webview: chrome.webviewTag.WebView|null = null;
  protected bridge: SkillsWebviewBridge|null = null;

  constructor(url: string) {
    this.url = url;
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
    };

    // Initiate handshake. Show error page on failure.
    this.bridge = new SkillsWebviewBridge(this.webview, delegate);
    this.webview.setAttribute('src', this.url);
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
