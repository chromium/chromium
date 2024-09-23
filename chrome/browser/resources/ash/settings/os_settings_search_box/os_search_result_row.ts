// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-search-result-row' is the container for one search result.
 */
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
// <if expr="_google_chrome">
import '/nearby/nearby-share-internal-icons.m.js';
// </if>
import '../os_settings_icons.html.js';
import '../settings_shared.css.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {FocusRowMixin} from 'chrome://resources/ash/common/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {SearchResult as PersonalizationSearchResult} from '../mojom-webui/personalization_search.mojom-webui.js';
import {Section, Subpage} from '../mojom-webui/routes.mojom-webui.js';
import {SearchResult as SettingsSearchResult, SearchResultIdentifier, SearchResultType} from '../mojom-webui/search.mojom-webui.js';
import {SearchResultIcon} from '../mojom-webui/search_result_icon.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Router} from '../router.js';
import {SearchResult} from '../search/combined_search_handler.js';

import {getTemplate} from './os_search_result_row.html.js';

/**
 * This solution uses DP and has the complexity of O(M*N), where M and N are
 * the lengths of |string1| and |string2| respectively.
 *
 * @param string1 The first case sensitive string to be compared.
 * @param string2 The second case sensitive string to be compared.
 * @return An array of the longest common substrings starting
 *     from the earliest to latest match, all of which have the same length.
 *     Returns empty array if there are none.
 */
function longestCommonSubstrings(string1: string, string2: string): string[] {
  let maxLength = 0;
  let string1StartingIndices: number[] = [];
  const dp = Array(string1.length + 1)
                 .fill([])
                 .map(() => Array(string2.length + 1).fill(0));

  for (let i = string1.length - 1; i >= 0; i--) {
    for (let j = string2.length - 1; j >= 0; j--) {
      if (string1[i] !== string2[j]) {
        continue;
      }
      dp[i][j] = dp[i + 1][j + 1] + 1;
      if (maxLength === dp[i][j]) {
        string1StartingIndices.unshift(i);
      }
      if (maxLength < dp[i][j]) {
        maxLength = dp[i][j];
        string1StartingIndices = [i];
      }
    }
  }

  return string1StartingIndices.map(idx => {
    return string1.substr(idx, maxLength);
  });
}

function isPersonalizationSearchResult(result: SearchResult):
    result is PersonalizationSearchResult {
  return !!result &&
      typeof (result as PersonalizationSearchResult).relativeUrl === 'string';
}

/**
 * Used to locate matches such that the query text omits a hyphen when the
 * matching result text contains a hyphen.
 */
const DELOCALIZED_HYPHEN = '-';

/**
 * A list of hyphens in all languages that will be ignored during the
 * tokenization and comparison of search result text.
 * Hyphen characters list is taken from here: http://jkorpela.fi/dashes.html.
 * U+002D(-), U+007E(~), U+058A(֊), U+05BE(־), U+1806(᠆), U+2010(‐),
 * U+2011(‑), U+2012(‒), U+2013(–), U+2014(—), U+2015(―), U+2053(⁓),
 * U+207B(⁻), U+208B(₋), U+2212(−), U+2E3A(⸺ ), U+2E3B(⸻  ), U+301C(〜),
 * U+3030(〰), U+30A0(゠), U+FE58(﹘), U+FE63(﹣), U+FF0D(－).
 */
const HYPHENS: string[] = [
  '-', '~', '֊', '־', '᠆', '‐',  '‑',  '‒',  '–',  '—',  '―',  '⁓',
  '⁻', '₋', '−', '⸺', '⸻', '〜', '〰', '゠', '﹘', '﹣', '－',
];

/**
 * String form of the regexp expressing hyphen chars.
 */
const HYPHENS_REGEX_STR = `[${HYPHENS.join('')}]`;

/**
 * Regexp expressing hyphen chars.
 */
const HYPHENS_REGEX = new RegExp(HYPHENS_REGEX_STR, 'g');

/**
 * @param sourceString The string to be modified.
 * @return The sourceString lowercased with accents in the range
 *     \u0300 - \u036f removed.
 */
function removeAccents(sourceString: string): string {
  return sourceString.toLocaleLowerCase().normalize('NFD').replace(
      /[\u0300-\u036f]/g, '');
}

/**
 * Used to convert the query and result into the same format without hyphens
 * and accents so that easy string comparisons can be performed. e.g.
 * |sourceString| = 'BRÛLÉE' returns "brulee"
 * @param sourceString The string to be normalized.
 * @return The sourceString lowercased with accents in the range
 *     \u0300 - \u036f removed, and with hyphens removed.
 */
function normalizeString(sourceString: string): string {
  return removeAccents(sourceString).replace(HYPHENS_REGEX, '');
}

/**
 * Bolds all strings in |substringsToBold| that occur in |sourceString|,
 * regardless of case.
 *     e.g. |sourceString| = "Turn on Wi-Fi"
 *          |substringsToBold| = ['o', 'wi-f', 'ur']
 *          returns 'T<b>ur</b>n <b>o</b>n <b>Wi-F</b>i'
 * @param sourceString The case sensitive string to be bolded.
 * @param substringsToBold The case-insensitive substrings
 *     that will be bolded in the |sourceString|, if they are html substrings
 *     of the |sourceString|.
 * @return An innerHTML string of |sourceString| with any
 *     |substringsToBold| regardless of case bolded.
 */
function boldSubStrings(
    sourceString: string, substringsToBold: string[]): string {
  if (!substringsToBold || !substringsToBold.length) {
    return sourceString;
  }
  const subStrRegex =
      new RegExp('(\)(' + substringsToBold.join('|') + ')(\)', 'ig');
  return sourceString.replace(subStrRegex, (match) => match.bold());
}

const OsSearchResultRowElementBase = FocusRowMixin(I18nMixin(PolymerElement));

export class OsSearchResultRowElement extends OsSearchResultRowElementBase {
  static get is() {
    return 'os-search-result-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Whether the search result row is selected. */
      selected: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'makeA11yAnnouncementIfSelectedAndUnfocused_',
      },

      /** Aria label for the row. */
      ariaLabel: {
        type: String,
        computed: 'computeAriaLabel_(searchResult)',
        reflectToAttribute: true,
      },

      /** The query used to fetch this result. */
      searchQuery: String,

      searchResult: Object,

      /** Number of rows in the list this row is part of. */
      listLength: Number,

      resultText_: {
        type: String,
        computed: 'computeResultText_(searchResult)',
      },
    };
  }

  selected: boolean;
  override ariaLabel: string;
  searchQuery: string;
  searchResult: SearchResult;
  listLength: number;
  private resultText_: string;

  private makeA11yAnnouncementIfSelectedAndUnfocused_(): void {
    if (!this.selected || this.lastFocused) {
      // Do not alert the user if the result is not selected, or
      // the list is focused, defer to aria tags instead.
      return;
    }

    // The selected item is normally not focused when selected, the
    // selected search result should be verbalized as it changes.
    getAnnouncerInstance().announce(this.ariaLabel);
  }

  private computeResultText_(): string {
    // The C++ layer stores the text result as an array of 16 bit char codes,
    // so it must be converted to a JS String.
    return mojoString16ToString(this.searchResult.text);
  }

  /**
   * Bolds individual characters in the result text that are characters in the
   * search query, regardless of order. Some languages represent words with
   * single characters and do not include spaces. In those instances, use
   * exact character matching.
   *     e.g |this.resultText_| = "一二三四"
   *         |this.searchQuery| = "三一"
   *         returns "<b>一</b>二<b>三</b>四"
   * @return An innerHTML string of |this.resultText_| with any
   *     character that is in |this.searchQuery| bolded.
   */
  private getMatchingIndividualCharsBolded_(): string {
    return boldSubStrings(
        /*sourceString=*/ this.resultText_,
        /*substringsToBold=*/ this.searchQuery.split(''));
  }

  /**
   * @param innerHtmlToken A case sensitive segment of the result
   *     text which may or may not contain hyphens or accents on
   *     characters, and does not contain blank spaces.
   * @param normalizedQuery A lowercased query which does not contain
   *     hyphens.
   * @param queryTokens See generateQueryTokens_().
   * @return The innerHtmlToken with <b> tags around segments that
   *     match queryTokens, but also includes hyphens and accents
   *     on characters.
   */
  private getModifiedInnerHtmlToken_(
      innerHtmlToken: string, normalizedQuery: string,
      queryTokens: string[]): string {
    // For comparison purposes with query tokens, lowercase the html token to
    // be displayed, remove hyphens, and remove accents. The resulting
    // |normalizedToken| will not be the displayed token.
    const normalizedToken = normalizeString(innerHtmlToken);
    if (normalizedQuery.includes(normalizedToken)) {
      // Bold the entire html token to be displayed, if the result is a
      // substring of the query, regardless of blank spaces that may or
      // may not have not been extraneous.
      return normalizedToken ? innerHtmlToken.bold() : innerHtmlToken;
    }

    // Filters out query tokens that are not substrings of the currently
    // processing text token to be displayed.
    const queryTokenFilter = (queryToken: string): boolean => {
      return !!queryToken && normalizedToken.includes(queryToken);
    };

    // Maps the queryToken to the segment(s) of the html token that contain
    // the queryToken interweaved with any of the hyphens that were
    // filtered out during normalization. For example, |innerHtmlToken| =
    // 'Wi-Fi-no-blankspsc-WiFi', (i.e. |normalizedToken| =
    // 'WiFinoblankspcWiFi') and |queryTokenLowerCaseNoSpecial| = 'wif', the
    // resulting mapping would be ['Wi-F', 'WiF'].
    const queryTokenToSegment = (queryToken: string): string[] => {
      const regExpStr = queryToken.split('').join(`${HYPHENS_REGEX_STR}*`);

      // Since |queryToken| does not contain accents and |innerHtmlToken| may
      // have accents matches must be made without accents on characters.
      const innerHtmlTokenNoAccents = removeAccents(innerHtmlToken);
      const matchesNoAccents: string[] =
          innerHtmlTokenNoAccents.match(new RegExp(regExpStr, 'g')) || [];

      // Return matches with original accents restored.
      return matchesNoAccents.map(
          match => innerHtmlToken.toLocaleLowerCase().substr(
              innerHtmlTokenNoAccents.indexOf(match), match.length));
    };

    // Contains lowercase segments of the innerHtmlToken that may or may not
    // contain hyphens and accents on characters.
    const matches =
        queryTokens.filter(queryTokenFilter).map(queryTokenToSegment).flat();

    if (!matches.length) {
      // No matches, return token to displayed as is.
      return innerHtmlToken;
    }

    // Get the length of the longest matched substring(s).
    const maxStrLen =
        matches.reduce((a, b) => a.length > b.length ? a : b).length;

    // Bold the longest substring(s).
    const bolded =
        matches.filter(sourceString => sourceString.length === maxStrLen);
    return boldSubStrings(
        /*sourceString=*/ innerHtmlToken, /*substringsToBold=*/ bolded);
  }

  /**
   * Query tokens are created first by splitting the |normalizedQuery| with
   * blankspaces into query segments. Then, each query segment is compared
   * to the the normalized result text (result text without hyphens or
   * accents). Query tokens are created by finding the longest common
   * substring(s) between a query segment and the normalized result text. Each
   * query segment is mapped to an array of their query tokens. Finally, the
   * longest query token(s) for each query segment are extracted. In the event
   * that query segments are more than one character long, query tokens that
   * are only one character long are ignored.
   * @param normalizedQuery A lowercased query which does not contain
   *     hyphens or accents.
   * @return QueryTokens that do not contain
   *     blankspaces and are substrings of the normalized result text
   */
  private generateQueryTokens_(normalizedQuery: string): string[] {
    const normalizedResultText = normalizeString(this.resultText_);

    const segmentToTokenMap = new Map<string, string[]>();
    normalizedQuery.split(/\s/).forEach(querySegment => {
      const queryTokens =
          longestCommonSubstrings(querySegment, normalizedResultText);
      if (segmentToTokenMap.has(querySegment)) {
        const segmentTokens =
            segmentToTokenMap.get(querySegment)!.concat(queryTokens);
        segmentToTokenMap.set(querySegment, segmentTokens);
        return;
      }
      segmentToTokenMap.set(querySegment, queryTokens);
    });

    // For each segment, only return the longest token. For example, in the
    // case that |resultText_| is "Search and Assistant", a |querySegment| key
    // of "ssistan" will yield a |queryToken| value array containing "ssistan"
    // (longest common substring for "Assistant") and "an" (longest common
    // substring for "and"). Only the queryToken "ssistan" should be kept
    // since it's the longest queryToken.
    const getLongestTokensPerSegment =
        ([querySegment, queryTokens]: [string, string[]]): string[] => {
          // If there are no queryTokens, return none.
          // Example: |normalizedResultText| = "search and assistant"
          //          |normalizedQuery| = "hi goog"
          //          |querySegment| = "goog"
          //          |queryTokens| = []
          // Since |querySegment| does not share any substrings with
          // |normalizedResultText|, no queryTokens available.
          if (!queryTokens.length) {
            return [];
          }

          const maxLengthQueryToken =
              Math.max(...queryTokens.map(queryToken => queryToken.length));

          // If the |querySegment| is more than one character long and the
          // longest queryToken(s) are one character long, discard all
          // queryToken(s). This prevents random single characters in in the
          // result text from bolding. Example: |normalizedResultText| = "search
          // and assistant"
          //          |normalizedQuery| = "hi goog"
          //          |querySegment| = "hi"
          //          |queryTokens| = ["h", "i"]
          // Here, |querySegment| "hi" shares a common substring "h" with
          // |normalizedResultText|'s "search" and "i" with
          // |normalizedResultText|'s "assistant". Since the queryTokens for
          // the length two querySegment are only one character long, discard
          // the queryTokens.
          if (maxLengthQueryToken === 1 && querySegment.length > 1) {
            return [];
          }

          return queryTokens.filter(
              queryToken => queryToken.length === maxLengthQueryToken);
        };

    // A 2D array such that each array contains queryTokens of a querySegment.
    // Note that the order of key value pairs is maintained in the
    // |segmentToTokenMap| relative to the |normalizedQuery|, and the order
    // of the queryTokens within each inner array is also maintained relative
    // to the |normalizedQuery|.
    const inOrderTokenGroups =
        Array.from(segmentToTokenMap).map(getLongestTokensPerSegment);

    // Flatten the 2D |inOrderTokenGroups|, and remove duplicate queryTokens.
    // Note that even though joining |inOrderTokens| will always form a
    // subsequence of |normalizedQuery|, it will not be a subsequence of
    // |normalizedResultText|.
    // Example: |this.resultText| = "Touchpad tap-to-click"
    //          |normalizedResultText| = "touchpad taptoclick"
    //          |normalizedQuery| = "tap to cli"
    //          |inOrderTokenGroups| = [['tap']. ['to', 'to']. ['cli']]
    //          |inOrderTokens| = ['tap', 'to', 'cli']
    // |inOrderTokenGroups| contains an inner array of two 'to's because
    // the |querySegment| = 'to' matches with 'touchpad' and 'taptoclick'.
    // Duplicate entries are removed in |inOrderTokens| because
    // if a |queryToken| is merged to form a compound worded queryToken, it
    // should not be used to bold another |resultText| word. In the fictitious
    // case that |inOrderTokenGroups| is [['tap']. ['to', 'xy']. ['cli']],
    // |inOrderTokens| will be ['tap', 'to', 'xy', 'cli'], and only 'Tap-to'
    // will be bolded. This is fine because 'toxy' is a subsequence of a
    // |querySegment| the user inputted, and the order of bolding
    // will prefer the user's input in these extenuating circumstances.
    const inOrderTokens = [...new Set(inOrderTokenGroups.flat())];
    return this.mergeValidTokensToCompounded_(inOrderTokens);
  }

  /**
   * Possibly merges costituent queryTokens in |inOrderQueryTokens| to form
   * new, longer, valid queryTokens that match with normalized compounded
   * words in |this.resultText|.
   * @param inOrderQueryTokens An array of valid queryTokens
   *     that do not contain dups.
   * @return An array of queryTokens of equal or lesser size
   *     than |inOrderQueryTokens|, each of which do not contain blankspaces
   *     and are substrings of the normalized result text.
   */
  private mergeValidTokensToCompounded_(inOrderQueryTokens: string[]):
      string[] {
    // If |this.resultToken| does not contain any hyphens, this will be
    // be the same as |inOrderQueryTokens|.
    const longestCompoundWordTokens: string[] = [];

    // Instead of stripping all hyphen as would be the case if the result
    // text were normalized, convert all hyphens to |DELOCALIZED_HYPHEN|. This
    // string will be compared with compound query tokens to find query tokens
    // that are compound substrings longer than the constituent query tokens.
    const hyphenatedResultText =
        removeAccents(this.resultText_)
            .replace(HYPHENS_REGEX, DELOCALIZED_HYPHEN);

    // Create the longest combined tokens delimited by |DELOCALIZED_HYPHEN|s
    // that are a substrings of |hyphenatedResultText|. Worst case visit each
    // token twice. Note that if a token is used to form a compound word, it
    // will no longer be present for other words.
    // Example: |this.resultText| = "Touchpad tap-to-click"
    //          |this.searchQuery| = "tap to clic"
    // The token "to" will fail to highlight "To" in "Touchpad", and instead
    // will be combined with "tap" and "clic" to bold "tap-to-click".
    let i = 0;
    while (i < inOrderQueryTokens.length) {
      let prefixToken = inOrderQueryTokens[i];
      i++;
      while (i < inOrderQueryTokens.length) {
        // Create a compound token with the next token within
        // |inOrderQueryTokens|.
        const compoundToken =
            prefixToken + DELOCALIZED_HYPHEN + inOrderQueryTokens[i];

        // If the constructed compoundToken from valid queryTokens is not a
        // substring of the |hyphenatedResultText|, break from the inner loop
        // and set the outer loop to start with the token that broke the
        // compounded match.
        if (!hyphenatedResultText.includes(compoundToken)) {
          break;
        }

        prefixToken = compoundToken;
        i++;
      }
      longestCompoundWordTokens.push(prefixToken);
    }

    // Normalize the compound tokens that include |DELOCALIZED_HYPHEN|s.
    return longestCompoundWordTokens.map(token => normalizeString(token));
  }

  /**
   * Tokenize the result and query text, and match the tokens even if they
   * are out of order. Both the result and query tokens are compared without
   * hyphens or accents on characters. Result text is simply tokenized by
   * blankspaces. On the other hand, query text is tokenized within
   * generateQueryTokens_(). As each result token is processed, it is compared
   * with every query token. Bold the segment of the result token that is a
   * query token. e.g. Smaller query block: if "wif on" is
   * queried, a result text of "Turn on Wi-Fi" should have "on" and "Wi-F"
   * bolded. e.g. Larger query block: If "onwifi" is queried, a result text of
   * "Turn on Wi-Fi" should have "Wi-Fi" bolded.
   * @return Result string with <b> tags around query sub string.
   */
  private getTokenizeMatchedBoldTagged_(): string {
    // Lowercase, remove hyphens, and remove accents from the query.
    const normalizedQuery = normalizeString(this.searchQuery);

    const queryTokens = this.generateQueryTokens_(normalizedQuery);

    // Get innerHtmlTokens with bold tags around matching segments.
    const innerHtmlTokensWithBoldTags = this.resultText_.split(/\s/).map(
        innerHtmlToken => this.getModifiedInnerHtmlToken_(
            innerHtmlToken, normalizedQuery, queryTokens));

    // Get all blankspace types.
    const blankspaces = this.resultText_.match(/\s/g);

    if (!blankspaces) {
      // No blankspaces, return |innterHtmlTokensWithBoldTags| as a string.
      return innerHtmlTokensWithBoldTags.join('');
    }

    // Add blankspaces make to where they were located in the string, and
    // form one string to be added to the html.
    // e.g |blankspaces| = [' ', '\xa0']
    //     |innerHtmlTokensWithBoldTags| = ['a', '<b>b</b>', 'c']
    // returns 'a <b>b</b>&nbps;c'
    return innerHtmlTokensWithBoldTags
        .map((token, idx) => {
          return idx !== blankspaces.length ? token + blankspaces[idx] : token;
        })
        .join('');
  }

  /**
   * @return The result string with <span> tags around keywords.
   */
  private getResultInnerHtml_(): TrustedHTML {
    if (!(this.searchResult as SettingsSearchResult)
             .wasGeneratedFromTextMatch) {
      return sanitizeInnerHtml(this.resultText_);
    }

    if (this.resultText_.match(/\s/) ||
        this.resultText_.toLocaleLowerCase() !==
            this.resultText_.toLocaleUpperCase()) {
      // If the result text includes blankspaces (as they commonly will in
      // languages like Arabic and Hindi), or if the result text includes
      // at least one character such that the lowercase is different from
      // the uppercase (as they commonly will in languages like English
      // and Russian), tokenize the result text by blankspaces, and bold based
      // off of matching substrings in the tokens.
      return sanitizeInnerHtml(this.getTokenizeMatchedBoldTagged_());
    }

    // If the result text does not contain blankspaces or characters that
    // have upper/lower case differentiation (as they commonly do in languages
    // like Chinese and Japanese), bold exact characters that match.
    return sanitizeInnerHtml(this.getMatchingIndividualCharsBolded_());
  }

  /**
   * @return Aria label string for ChromeVox to verbalize.
   */
  private computeAriaLabel_(): string {
    assert(typeof this.focusRowIndex === 'number');
    return this.i18n(
        'searchResultSelected', this.focusRowIndex + 1, this.listLength,
        this.computeResultText_());
  }

  /**
   * Only relevant when the focus-row-control is focus()ed. This keypress
   * handler specifies that pressing 'Enter' should cause a route change.
   */
  private onKeyPress_(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      e.stopPropagation();
      this.onSearchResultSelected();
    }
  }

  private recordSearchResultMetrics_(): void {
    if (isPersonalizationSearchResult(this.searchResult)) {
      chrome.metricsPrivate.recordSparseValue(
          'ChromeOS.Settings.SearchResultPersonalizationSelected',
          this.searchResult.searchConceptId);
      // Record entry point metric to Personalization Hub through Settings
      // search.
      chrome.metricsPrivate.recordEnumerationValue(
          'Ash.Personalization.EntryPoint',
          loadTimeData.getInteger('settingsSearchEntryPoint'),
          loadTimeData.getInteger('entryPointEnumSize'));
      return;
    }

    const settingsSearchResult = this.searchResult as SettingsSearchResult;
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.Settings.SearchResultTypeSelected', settingsSearchResult.type,
        SearchResultType.MAX_VALUE);

    interface MetricArg {
      metricName: string;
      value?: Section|Subpage|Setting;
    }
    const metricArgs =
        (type: number, id: SearchResultIdentifier): MetricArg => {
          switch (type) {
            case SearchResultType.kSection:
              return {
                metricName: 'ChromeOS.Settings.SearchResultSectionSelected',
                value: id.section,
              };
            case SearchResultType.kSubpage:
              return {
                metricName: 'ChromeOS.Settings.SearchResultSubpageSelected',
                value: id.subpage,
              };
            case SearchResultType.kSetting:
              return {
                metricName: 'ChromeOS.Settings.SearchResultSettingSelected',
                value: id.setting,
              };
            default:
              assertNotReached('Search Result Type not specified.');
          }
        };

    const args =
        metricArgs(settingsSearchResult.type, settingsSearchResult.id)!;
    if (args.value) {
      chrome.metricsPrivate.recordSparseValue(args.metricName, args.value);
    }
  }

  /**
   * Navigate to a search result route or launch an external url based on
   * the search result's id.
   */
  onSearchResultSelected(): void {
    if (isPersonalizationSearchResult(this.searchResult)) {
      this.recordSearchResultMetrics_();
      OpenWindowProxyImpl.getInstance().openUrl(
          loadTimeData.getString('personalizationAppUrl') +
          this.searchResult.relativeUrl);
      return;
    }

    const settingsSearchResult = this.searchResult as SettingsSearchResult;
    assert(settingsSearchResult.urlPathWithParameters, 'Url path is empty.');
    this.recordSearchResultMetrics_();

    // |this.searchResult.urlPathWithParameters| separates the path and params
    // by a '?' char.
    const pathAndOptParams =
        settingsSearchResult.urlPathWithParameters.split('?');

    // There should be at most 2 items in the array (the path and the params).
    assert(pathAndOptParams.length <= 2, 'Path and params format error.');

    const route =
        Router.getInstance().getRouteForPath('/' + pathAndOptParams[0]);
    assert(
        route,
        'Supplied path does not map to an existing route: ' +
            pathAndOptParams[0]);

    const paramsString = `search=${encodeURIComponent(this.searchQuery)}` +
        (pathAndOptParams.length === 2 ? `&${pathAndOptParams[1]}` : ``);
    const params = new URLSearchParams(paramsString);
    Router.getInstance().navigateTo(route, params);

    const event = new CustomEvent(
        'navigated-to-result-route', {bubbles: true, composed: true});
    this.dispatchEvent(event);
  }

  /**
   * @return The name of the icon to use.
   */
  private getResultIcon_(): string {
    const isRevampEnabled = isRevampWayfindingEnabled();
    if (isPersonalizationSearchResult(this.searchResult)) {
      return isRevampEnabled ? 'os-settings:personalization-revamp' :
                               'os-settings:paint-brush';
    }

    const settingsSearchResult = this.searchResult as SettingsSearchResult;
    switch (settingsSearchResult.icon) {
      case SearchResultIcon.kA11y:
        return isRevampEnabled ? 'os-settings:accessibility-revamp' :
                                 'os-settings:accessibility';
      case SearchResultIcon.kAndroid:
        return 'os-settings:android';
      case SearchResultIcon.kAppsParentalControls:
        return 'os-settings:apps-parental-controls';
      case SearchResultIcon.kAppsGrid:
        return 'os-settings:apps';
      case SearchResultIcon.kAssistant:
        return 'os-settings:assistant';
      case SearchResultIcon.kAudio:
        return isRevampEnabled ? 'os-settings:device-audio' :
                                 'os-settings:audio';
      case SearchResultIcon.kAuthKey:
        return 'os-settings:auth-key';
      case SearchResultIcon.kAutoclick:
        return 'os-settings:autoclick';
      case SearchResultIcon.kSwitchAccess:
        return 'os-settings:switch-access';
      case SearchResultIcon.kAvatar:
        return isRevampEnabled ? 'os-settings:privacy-manage-people' :
                                 'cr:person';
      case SearchResultIcon.kBluetooth:
        return 'cr:bluetooth';
      case SearchResultIcon.kCamera:
        return 'os-settings:camera';
      case SearchResultIcon.kCellular:
        return 'os-settings:cellular';
      case SearchResultIcon.kCheckForUpdate:
        return 'os-settings:about-update-complete';
      case SearchResultIcon.kChrome:
        return 'os-settings:chrome';
      case SearchResultIcon.kClock:
        return 'os-settings:clock';
      case SearchResultIcon.kContrast:
        return 'os-settings:contrast';
      case SearchResultIcon.kCursorClick:
        return 'os-settings:cursor-click';
      case SearchResultIcon.kDetailedBuild:
        return 'os-settings:about-additional-details';
      case SearchResultIcon.kDeveloperTags:
        return 'os-settings:developer-tags';
      case SearchResultIcon.kDiagnostics:
        return 'os-settings:about-diagnostics';
      case SearchResultIcon.kDictation:
        return 'os-settings:dictation';
      case SearchResultIcon.kDisplay:
        return isRevampEnabled ? 'os-settings:device-display' :
                                 'os-settings:display';
      case SearchResultIcon.kDockedMagnifier:
        return 'os-settings:docked-magnifier';
      case SearchResultIcon.kEthernet:
        return 'os-settings:settings-ethernet';
      case SearchResultIcon.kFingerprint:
        return 'os-settings:fingerprint';
      case SearchResultIcon.kFirmwareUpdates:
        return 'os-settings:about-firmware-updates';
      case SearchResultIcon.kFolder:
        return 'os-settings:folder-outline';
      case SearchResultIcon.kFolderShared:
        return 'os-settings:folder-shared';
      case SearchResultIcon.kFullscreenMagnifier:
        return 'os-settings:fullscreen-magnifier';
      case SearchResultIcon.kGeolocation:
        return 'os-settings:geolocation';
      case SearchResultIcon.kGoogleDrive:
        return isRevampEnabled ? 'os-settings:google-drive-revamp' :
                                 'os-settings:google-drive';
      case SearchResultIcon.kGooglePlay:
        return isRevampEnabled ? 'os-settings:google-play-revamp' :
                                 'os-settings:google-play';
      case SearchResultIcon.kHearing:
        return 'os-settings:a11y-hearing';
      case SearchResultIcon.kHelp:
        return 'os-settings:about-help';
      case SearchResultIcon.kHelpMeRead:
        return 'os-settings:help-me-read';
      case SearchResultIcon.kHelpMeWrite:
        return 'os-settings:help-me-write';
      case SearchResultIcon.kHotspot:
        return 'os-settings:hotspot';
      case SearchResultIcon.kInstantTethering:
        return 'os-settings:magic-tethering';
      case SearchResultIcon.kKeyboard:
        return isRevampEnabled ? 'os-settings:device-keyboard' :
                                 'os-settings:keyboard';
      case SearchResultIcon.kLanguage:
        return isRevampEnabled ? 'os-settings:language-revamp' :
                                 'os-settings:language';
      case SearchResultIcon.kLaptop:
        return 'os-settings:laptop-chromebook';
      case SearchResultIcon.kLock:
        return isRevampEnabled ? 'os-settings:lock-revamp' : 'os-settings:lock';
      case SearchResultIcon.kMagicBoost:
        return 'os-settings:magic-boost';
      case SearchResultIcon.kMicrophone:
        return 'os-settings:microphone';
      case SearchResultIcon.kMouse:
        return isRevampEnabled ? 'os-settings:device-mouse' :
                                 'os-settings:mouse';
      case SearchResultIcon.kNearbyShare:
        // <if expr="_google_chrome">
        if (loadTimeData.getBoolean('isNameEnabled')) {
          return 'nearby-share-internal:nearby-share';
        }
        // </if>
        return 'os-settings:nearby-share';
      case SearchResultIcon.kNotifications:
        return 'os-settings:apps-notifications';
      case SearchResultIcon.kOneDrive:
        return 'settings20:onedrive';
      case SearchResultIcon.kOnScreenKeyboard:
        return 'os-settings:on-screen-keyboard';
      case SearchResultIcon.kPaintbrush:
        return isRevampEnabled ? 'os-settings:personalization-revamp' :
                                 'os-settings:paint-brush';
      case SearchResultIcon.kPenguin:
        return 'os-settings:crostini-mascot';
      case SearchResultIcon.kPersonalization:
        return 'os-settings:personalization';
      case SearchResultIcon.kPhone:
        return isRevampEnabled ?
            'os-settings:connected-devices-android-phone' :
            'os-settings:multidevice-better-together-suite';
      case SearchResultIcon.kPluginVm:
        return 'os-settings:plugin-vm';
      case SearchResultIcon.kPointingStick:
        return 'os-settings:device-pointing-stick';
      case SearchResultIcon.kPower:
        return 'os-settings:power';
      case SearchResultIcon.kPrinter:
        return isRevampEnabled ? 'os-settings:device-print' :
                                 'os-settings:print';
      case SearchResultIcon.kPrivacyControls:
        return 'os-settings:privacy-controls';
      case SearchResultIcon.kReducedAnimations:
        return 'os-settings:reduced-animations';
      case SearchResultIcon.kReleaseNotes:
        return 'os-settings:about-release-notes';
      case SearchResultIcon.kReset:
        return isRevampEnabled ? 'os-settings:startup' : 'os-settings:restore';
      case SearchResultIcon.kRestore:
        return isRevampEnabled ? 'os-settings:restore-revamp' :
                                 'os-settings:startup';
      case SearchResultIcon.kScanner:
        return 'os-settings:device-scan';
      case SearchResultIcon.kSearch:
        return isRevampEnabled ? 'os-settings:explore' : 'cr:search';
      case SearchResultIcon.kSelectToSpeak:
        return 'os-settings:select-to-speak';
      case SearchResultIcon.kShield:
        return 'cr:security';
      case SearchResultIcon.kSnapWindowSuggestions:
        return 'os-settings:snap-window-suggestions';
      case SearchResultIcon.kStorage:
        return isRevampEnabled ? 'os-settings:storage' :
                                 'os-settings:hard-drive';
      case SearchResultIcon.kStylus:
        return isRevampEnabled ? 'os-settings:device-stylus' :
                                 'os-settings:stylus';
      case SearchResultIcon.kSync:
        return isRevampEnabled ? 'os-settings:sync-revamp' : 'os-settings:sync';
      case SearchResultIcon.kSystemPreferences:
        return 'os-settings:system-preferences';
      case SearchResultIcon.kTextToSpeech:
        return 'os-settings:text-to-speech';
      case SearchResultIcon.kTouchpad:
        return 'os-settings:device-touchpad';
      case SearchResultIcon.kWallpaper:
        return 'os-settings:wallpaper';
      case SearchResultIcon.kWifi:
        return 'os-settings:network-wifi';
      case SearchResultIcon.kZoomIn:
        return 'os-settings:zoom-in';
      default:
        return 'os-settings:settings-general';
    }
  }

  private getActionTypeIcon_(): string {
    return isPersonalizationSearchResult(this.searchResult) ?
        'cr:open-in-new' :
        'cr:arrow-forward';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-search-result-row': OsSearchResultRowElement;
  }
}

customElements.define(OsSearchResultRowElement.is, OsSearchResultRowElement);
