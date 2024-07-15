// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl} from './browser_proxy.js';
import {UserAction} from './lens.mojom-webui.js';

export function recordLensOverlayInteraction(
    invocationSource: string, interaction: UserAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'Lens.Overlay.Overlay.UserAction', interaction, UserAction.MAX_VALUE + 1);
  chrome.metricsPrivate.recordEnumerationValue(
      `Lens.Overlay.Overlay.ByInvocationSource.${invocationSource}.UserAction`,
      interaction, UserAction.MAX_VALUE + 1);
  BrowserProxyImpl.getInstance().handler.recordUkmLensOverlayInteraction(
      interaction);
}
