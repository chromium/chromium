// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Token} from '//resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import type {OrganizerListSectionItemDescriptionPart} from '../organizer_list_section_item.js';

export function tokenToString(token: Token): string {
  return `${token.high.toString()}#${token.low.toString()}`;
}

export function getHostnameOrUrl(url: string): string {
  try {
    return new URL(url).hostname;
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

export function getTabDescriptionParts(
    urls: string[],
    lastActiveElapsedText?: string): OrganizerListSectionItemDescriptionPart[] {
  const description: OrganizerListSectionItemDescriptionPart[] =
      urls.map(url => ({
                 text: getHostnameOrUrl(url),
                 elideFromStart: true,
               }));
  if (lastActiveElapsedText) {
    description.push({text: lastActiveElapsedText});
  }
  return description;
}
