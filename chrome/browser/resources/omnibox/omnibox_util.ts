// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Signals} from './omnibox.mojom-webui.js';

export function clearChildren(element: Element) {
  while (element.firstChild) {
    element.firstChild.remove();
  }
}

export function createEl<K extends keyof HTMLElementTagNameMap>(
    tagName: K, parentEl: Element|null = null, classes: string[] = [],
    text: string = ''): HTMLElementTagNameMap[K] {
  const el = document.createElement(tagName);
  el.classList.add(...classes);
  el.textContent = text;
  if (parentEl) {
    parentEl.appendChild(el);
  }
  return el;
}

// Keep consistent:
// - omnibox_event.proto `ScoringSignals`
// - autocomplete_scoring_model_handler.cc
//   `AutocompleteScoringModelHandler::ExtractInputFromScoringSignals()`
// - autocomplete_match.cc `AutocompleteMatch::MergeScoringSignals()`
// - omnibox.mojom `struct Signals`
// - omnibox_page_handler.cc `TypeConverter<AutocompleteMatch::ScoringSignals,
//   mojom::SignalsPtr>`
// - omnibox_page_handler.cc `TypeConverter<mojom::SignalsPtr,
//   AutocompleteMatch::ScoringSignals>`
// - omnibox_util.ts `signalNames`
export const signalNames: Array<keyof Signals> = [
  'typedCount',
  'visitCount',
  'elapsedTimeLastVisitSecs',
  'shortcutVisitCount',
  'shortestShortcutLen',
  'elapsedTimeLastShortcutVisitSec',
  'isHostOnly',
  'numBookmarksOfUrl',
  'firstBookmarkTitleMatchPosition',
  'totalBookmarkTitleMatchLength',
  'numInputTermsMatchedByBookmarkTitle',
  'firstUrlMatchPosition',
  'totalUrlMatchLength',
  'hostMatchAtWordBoundary',
  'totalHostMatchLength',
  'totalPathMatchLength',
  'totalQueryOrRefMatchLength',
  'totalTitleMatchLength',
  'hasNonSchemeWwwMatch',
  'numInputTermsMatchedByTitle',
  'numInputTermsMatchedByUrl',
  'lengthOfUrl',
  'siteEngagement',
  'allowedToBeDefaultMatch',
];

export function clamp(value: number, min: number, max: number) {
  return Math.min(Math.max(value, min), max);
}

export class MlVersionObj {
  version: number;
  string: string;
  url: string;

  constructor(version: number) {
    this.version = version;
    this.string = version === -1 ?
        String(version) :
        `${version} (${new Date(version * 1000).toLocaleDateString()})`;
    const codeSearchPrefix =
        'https://source.corp.google.com/search?q=file:google3/googledata/chrome/breve/cacao/models/data/omnibox/url_scoring/';
    this.url = `${codeSearchPrefix} ${version}`;
  }
}
