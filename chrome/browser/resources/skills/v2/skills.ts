// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {getRequiredElement} from '//resources/js/util.js';

import {ErrorType} from '../error_page.js';
import {SkillsPageHandler} from '../skills.mojom-webui.js';

import type {SkillsWebviewBridgeDelegate} from './skills_webview_bridge.js';
import {SkillsWebviewBridge} from './skills_webview_bridge.js';
import {SKILLS_HOST_URL} from './skills_webview_bridge_constants.js';

const handler = SkillsPageHandler.getRemote();

function showError(webview: chrome.webviewTag.WebView, errorType: ErrorType) {
  const errorPage = document.querySelector('error-page');
  if (errorPage) {
    errorPage.errorType = errorType;
    errorPage.removeAttribute('hidden');
  }
  webview.setAttribute('hidden', 'true');
}

async function init() {
  const webview = getRequiredElement<chrome.webviewTag.WebView>('webview');

  // Wait for cookie sync to complete before setting src
  const {success} = await handler.syncCookies();
  if (!success) {
    showError(webview, ErrorType.GLIC_NOT_ENABLED);
    return;
  }

  const delegate: SkillsWebviewBridgeDelegate = {
    onError: () => showError(webview, ErrorType.REMOTE_AUTHORITY_UNREACHABLE),
    onShowToast: (toastType) => handler.showToast(toastType),
  };

  // Initiate handshake. Show error page on failure.
  new SkillsWebviewBridge(webview, delegate);
  webview.setAttribute('src', SKILLS_HOST_URL);
}

init();
