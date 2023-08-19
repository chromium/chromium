// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(cambickel): Move this code into a shared location, and update Settings
// search code to use it.

import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

/**
 * Returns the HTML for the given description based on the query text.
 * <b> tags are wrapped around matching sections of the description.
 * For example, a query "open tab" and a description "Open new tab" would return
 * "<b>Open</b> new <b>tab</b>".
 * @param description Search result description to apply bolding to.
 * @param query The text that the user searched for to generate this result.
 * @return A TrustedHTML of |description| with any character that is in
 *     |query| bolded.
 */
export function getBoldedDescription(
    description: string, query: string): TrustedHTML {
  if (description.match(/\s/) ||
      description.toLocaleLowerCase() !== description.toLocaleUpperCase()) {
    // If the result text includes blankspaces (as they commonly will in
    // languages like Arabic and Hindi), or if the result text includes
    // at least one character such that the lowercase is different from
    // the uppercase (as they commonly will in languages like English
    // and Russian), tokenize the result text by blankspaces, and bold based
    // off of matching substrings in the tokens.
    return getTokenizeMatchedBoldTagged(description, query);
  }

  // If the result text does not contain blankspaces or characters that
  // have upper/lower case differentiation (as they commonly do in languages
  // like Chinese and Japanese), bold exact characters that match.
  return getMatchingIndividualCharsBolded(description, query);
}

/**
 * Tokenize the result and query text, and match the tokens even if they
 * are out of order. Both the result and query tokens are compared without
 * hyphens or accents on characters. Result text is simply tokenized by
 * blankspaces. On the other hand, query text is tokenized within
 * generateQueryTokens(). As each result token is processed, it is compared
 * with every query token. Bold the segment of the result token that is a
 * query token. e.g. Smaller query block: if "wif on" is
 * queried, a result text of "Turn on Wi-Fi" should have "on" and "Wi-F"
 * bolded. e.g. Larger query block: If "onwifi" is queried, a result text of
 * "Turn on Wi-Fi" should have "Wi-Fi" bolded.
 * @param description Search result description to apply bolding to.
 * @param query The text that the user searched for to generate this result.
 * @return Result TrustedHTML with <b> tags around query sub string.
 */
function getTokenizeMatchedBoldTagged(
    description: string, query: string): TrustedHTML {
  // Lowercase, remove hyphens, and remove accents from the query.
  const normalizedQuery = normalizeString(query);

  const queryTokens = generateQueryTokens(description, normalizedQuery);

  // Get innerHtmlTokens with bold tags around matching segments.
  const innerHtmlTokensWithBoldTags = description.split(/\s/).map(
      innerHtmlToken => getModifiedInnerHtmlToken(
          innerHtmlToken, normalizedQuery, queryTokens));

  // Get all blankspace types.
  const blankspaces = description.match(/\s/g);

  if (!blankspaces) {
    // No blankspaces, return |innterHtmlTokensWithBoldTags| as a string.
    return sanitizeInnerHtml(innerHtmlTokensWithBoldTags.join(''));
  }

  // Add blankspaces make to where they were located in the string, and
  // form one string to be added to the html.
  // e.g |blankspaces| = [' ', '\xa0']
  //     |innerHtmlTokensWithBoldTags| = ['a', '<b>b</b>', 'c']
  // returns 'a <b>b</b>&nbps;c'
  return sanitizeInnerHtml(innerHtmlTokensWithBoldTags
                               .map((token, idx) => {
                                 return idx !== blankspaces.length ?
                                     token + blankspaces[idx] :
                                     token;
                               })
                               .join(''));
}

/**
 * Bolds individual characters in the result text that are characters in the
 * search query, regardless of order. Some languages represent words with
 * single characters and do not include spaces. In those instances, use
 * exact character matching.
 *     e.g |description| = "一二三四"
 *         |query| = "三一"
 *         returns "<b>一</b>二<b>三</b>四"
 * @param description Search result description to apply bolding to.
 * @param query The text that the user searched for to generate this result.
 * @return A TrustedHTML string of |description| with any character that is in
 *     |query| bolded.
 */
function getMatchingIndividualCharsBolded(
    description: string, query: string): TrustedHTML {
  return sanitizeInnerHtml(boldSubStrings(
      /*sourceString=*/ description,
      /*substringsToBold=*/ query.split('')));
}

/**
 * @param innerHtmlToken A case sensitive segment of the result
 *     text which may or may not contain hyphens or accents on
 *     characters, and does not contain blank spaces.
 * @param normalizedQuery A lowercased query which does not contain
 *     hyphens.
 * @param queryTokens See generateQueryTokens().
 * @return The innerHtmlToken with <b> tags around segments that
 *     match queryTokens, but also includes hyphens and accents
 *     on characters.
 */
function getModifiedInnerHtmlToken(
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
 * @param description Search result description to apply bolding to.
 * @param normalizedQuery A lowercased query which does not contain
 *     hyphens or accents.
 * @return QueryTokens that do not contain
 *     blankspaces and are substrings of the normalized result text
 */
function generateQueryTokens(
    description: string, normalizedQuery: string): string[] {
  const normalizedResultText = normalizeString(description);

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
  // Example: |description| = "Touchpad tap-to-click"
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
  return mergeValidTokensToCompounded(description, inOrderTokens);
}

/**
 * Possibly merges costituent queryTokens in |inOrderQueryTokens| to form
 * new, longer, valid queryTokens that match with normalized compounded
 * words in |description|.
 * @param description Search result description to apply bolding to.
 * @param inOrderQueryTokens An array of valid queryTokens
 *     that do not contain dups.
 * @return An array of queryTokens of equal or lesser size
 *     than |inOrderQueryTokens|, each of which do not contain blankspaces
 *     and are substrings of the normalized result text.
 */
function mergeValidTokensToCompounded(
    description: string, inOrderQueryTokens: string[]): string[] {
  // If |description| does not contain any hyphens, this will be
  // be the same as |inOrderQueryTokens|.
  const longestCompoundWordTokens: string[] = [];

  // Instead of stripping all hyphen as would be the case if the result
  // text were normalized, convert all hyphens to |DELOCALIZED_HYPHEN|. This
  // string will be compared with compound query tokens to find query tokens
  // that are compound substrings longer than the constituent query tokens.
  const hyphenatedResultText =
      removeAccents(description).replace(HYPHENS_REGEX, DELOCALIZED_HYPHEN);

  // Create the longest combined tokens delimited by |DELOCALIZED_HYPHEN|s
  // that are a substrings of |hyphenatedResultText|. Worst case visit each
  // token twice. Note that if a token is used to form a compound word, it
  // will no longer be present for other words.
  // Example: |description| = "Touchpad tap-to-click"
  //          |query| = "tap to clic"
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

/**
 * Used to locate matches such that the query text omits a hyphen when the
 * matching result text contains a hyphen.
 */
const DELOCALIZED_HYPHEN: string = '-';

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
const HYPHENS_REGEX_STR: string = `[${HYPHENS.join('')}]`;

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
