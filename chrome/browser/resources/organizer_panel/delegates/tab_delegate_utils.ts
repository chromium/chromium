// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';
import '../organizer_list_section_item.js';

import {CrUrlListItemSize} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {Token} from '//resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import type {OrganizerListSectionItem, OrganizerListSectionItemIcon} from '../organizer_list_section_item.js';
import {OrganizerListSectionItemIconType} from '../organizer_list_section_item.js';
import {SplitTabLayout} from '../tab_search.mojom-webui.js';

export function tokenToString(token: Token): string {
  return `${token.high.toString()}#${token.low.toString()}`;
}

export function getHostnameOrUrl(url: string): string {
  try {
    return new URL(url).hostname || url;
  } catch {
    return url;
  }
}

export function compareTimeDescending(
    timeA: {internalValue: bigint}|null|undefined,
    timeB: {internalValue: bigint}|null|undefined): number {
  const valA = timeA?.internalValue ?? 0n;
  const valB = timeB?.internalValue ?? 0n;
  return valB > valA ? 1 : (valB < valA ? -1 : 0);
}

export function getTabCountText(tabCount: number): string {
  return loadTimeData.getStringF(
      tabCount === 1 ? 'oneTab' : 'tabCount', tabCount);
}

export function getTabPrefixIcon(url: string): OrganizerListSectionItemIcon|
    undefined {
  if (!url) {
    return undefined;
  }
  return {type: OrganizerListSectionItemIconType.FAVICON, url};
}

export function getSplitViewPrefixIcon(
    urls: [string, string],
    layout: SplitTabLayout|null): OrganizerListSectionItemIcon|undefined {
  const validUrls = urls.filter(Boolean);
  if (validUrls.length < 2) {
    return getTabPrefixIcon(validUrls[0] ?? '');
  }
  return {
    type: OrganizerListSectionItemIconType.STACKED_FAVICONS,
    urls: [validUrls[0]!, validUrls[1]!],
    stackVertically: layout === SplitTabLayout.kStacked,
  };
}

export function removeListeners(
    callbackRouter: {removeListener(id: number): unknown},
    listenerIds: number[]): void {
  for (const id of listenerIds) {
    callbackRouter.removeListener(id);
  }
  listenerIds.length = 0;
}

export function getCompactZeroState(
    title: string, icon: string, onClick: () => void): TemplateResult {
  const item: OrganizerListSectionItem<unknown> = {
    title: [title],
    prefixIcon: {
      type: OrganizerListSectionItemIconType.ELEMENT,
      element: html`<cr-icon icon="${icon}"></cr-icon>`,
    },
    size: CrUrlListItemSize.COMPACT,
  };
  return html`
    <organizer-list-section-item .item="${item}" @click="${onClick}">
    </organizer-list-section-item>
  `;
}
