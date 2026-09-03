// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Range} from '/tab_search/shared/search.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {highlight} from 'chrome://resources/js/search_highlight_utils.js';

import type {Tab} from './tab_search.mojom-webui.js';
import {TabAlertState} from './tabs.mojom-webui.js';

export function highlightText(
    container: HTMLElement, text: string, ranges: Range[]|undefined) {
  container.textContent = '';
  const node = document.createTextNode(text);
  container.appendChild(node);
  if (ranges) {
    highlight(node, ranges);
  }
}

export function tabHasMediaAlerts(tab: Tab): boolean {
  return tab.alertStates.length > 0;
}

export function getMediaAlertImageClass(tab: Tab): string {
  if (!tabHasMediaAlerts(tab)) {
    return '';
  }
  const alert = tab.alertStates[0];
  switch (alert) {
    case TabAlertState.kMediaRecording:
      return 'media-recording';
    case TabAlertState.kAudioRecording:
      return 'audio-recording';
    case TabAlertState.kVideoRecording:
      return 'video-recording';
    case TabAlertState.kAudioPlaying:
      return 'audio-playing';
    case TabAlertState.kAudioMuting:
      return 'audio-muting';
    case TabAlertState.kGlicAccessing:
      return 'glic-accessing';
    default:
      return '';
  }
}

export function getFaviconUrlForTab(url: string, tab?: Tab|null): string {
  if (tab?.faviconUrl) {
    return `url("${tab.faviconUrl}")`;
  }
  const faviconPageUrl = tab?.isDefaultFavicon ? 'chrome://newtab' : url;
  return getFaviconForPageURL(faviconPageUrl, false);
}
