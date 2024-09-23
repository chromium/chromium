// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Signals} from './omnibox_internals.mojom-webui.js';

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
// - omnibox_scoring_signals.proto `OmniboxScoringSignals`
// - autocomplete_scoring_model_handler.cc
//   `AutocompleteScoringModelHandler::ExtractInputFromScoringSignals()`
// - autocomplete_match.cc `AutocompleteMatch::MergeScoringSignals()`
// - autocomplete_controller.cc `RecordScoringSignalCoverageForProvider()`
// - omnibox_metrics_provider.cc `GetScoringSignalsForLogging()`
// - omnibox.mojom `struct Signals`
// - omnibox_page_handler.cc
//   `TypeConverter<AutocompleteMatch::ScoringSignals, mojom::SignalsPtr>`
// - omnibox_page_handler.cc `TypeConverter<mojom::SignalsPtr,
//   AutocompleteMatch::ScoringSignals>`
// - omnibox_util.ts `signalNames`
// - omnibox/histograms.xml
//   `Omnibox.URLScoringModelExecuted.ScoringSignalCoverage`
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
  'searchSuggestRelevance',
  'isSearchSuggestEntity',
  'isVerbatim',
  'isNavsuggest',
  'isSearchSuggestTail',
  'isAnswerSuggest',
  'isCalculatorSuggest',
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

function setFormattedClipboard(text: string) {
  let styleCount = 0;
  const addStyle = (style: string) => {
    styleCount++;
    return `<span style="${style}">`;
  };
  const clearStyles = () => {
    const n = styleCount;
    styleCount = 0;
    return '</span>'.repeat(n);
  };
  let linkStep = 0;
  const addLink = () => {
    linkStep = (linkStep + 1) % 3;
    switch (linkStep) {
      case 1:
        return '<a href="';
      case 2:
        return '">';
      case 3:
      default:
        return '</a>';
    }
  };

  type StyleMap = Record<string, () => string>;
  const htmlMap: StyleMap = {
    $$: () => '$',
    $n: () => '<br>',
    $h: () => addStyle('font-weight:bold'),
    $r: () => addStyle('color:red'),
    $g: () => addStyle('color:green'),
    $b: () => addStyle('color:blue'),
    $p: () => addStyle('color:purple'),
    $0: clearStyles,
    $l: addLink,
  };
  const plainMap: StyleMap = {
    $$: () => '$',
    $n: () => '\n',
  };
  const applyMap = (map: StyleMap) => {
    return text.split(/(\$.)/g)
        .map((part, i) => i % 2 ? map[part]?.() ?? '' : part)
        .join('');
  };

  const clipboardEntries: Array<[string, StyleMap]> = [
    ['text/html', htmlMap],
    ['text/plain', plainMap],
  ];
  const clipboardItem =
      new ClipboardItem(Object.fromEntries(clipboardEntries.map(
          ([type, map]) => [type, new Blob([applyMap(map)], {type})])));
  return navigator.clipboard.write([clipboardItem]);
}

export function setFormattedClipboardForMl(
    matchDetails: Record<string, any>, signals: Signals, shareUrl: string,
    version: MlVersionObj) {
  return setFormattedClipboard([
    // clang-format off
    ...Object.entries(matchDetails)
        .flatMap(([k, v]) => ['$h$g', k, ': $0$b', v, '$0$n']),
    ...Object.entries(signals)
        .filter(([, v]) => v)
        .flatMap(([k, v]) => ['$h$p', k, ': $0$b', v, '$0$n']),
    ...shareUrl ? ['$r', shareUrl, '$0$n'] : [],
    '$h$r', 'Version: ', '$0', '$l', version.url, '$l', version.string, '$l$n',
    // clang-format on
  ].join(''));
}
