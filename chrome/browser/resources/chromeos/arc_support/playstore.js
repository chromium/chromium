// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns the Play Store footer element that can be detected by id or class
 * name.
 */
function getPlayFooterElement() {
  const elements = document.getElementsByClassName('glue-footer');
  if (!elements || elements.length === 0) {
    console.error('Failed to find play-footer element in ToS.');
    return null;
  }
  if (elements.length !== 1) {
    console.error('Found more than one play-footer element in ToS.');
  }
  return elements[0];
}

/**
 * Returns the select element that controls zone/language selection.
 */
function getLangZoneSelect() {
  const footer = getPlayFooterElement();
  if (!footer) {
    return null;
  }

  const elements = footer.getElementsByTagName('select');
  if (!elements || elements.length === 0) {
    console.error('Cannot find zone/language select select element');
    return null;
  }
  if (elements.length !== 1) {
    console.error('Found more than one zone/language select element in ToS.');
  }
  return elements[0];
}


/**
 * Analyzes current document and tries to find the link to the Play Store ToS
 * that matches requested |language| and |countryCode|. Once found, navigate
 * to this link and returns True. If no match was found then returns False.
 */
function navigateToLanguageAndCountryCode(language, countryCode) {
  const selectLangZoneTerms = getLangZoneSelect();

  if (!selectLangZoneTerms) {
    // Layout is not recognized, cannot check document structure.
    return false;
  }

  const applyTermsForLangAndZone = function(termsLang) {
    // Check special case for en_us which may be mapped to en.
    const matchDefaultUs = null;
    if (window.location.href.startsWith(
            'https://play.google/intl/en_us/play-terms') &&
        termsLang === 'en' && countryCode === 'us' &&
        selectLangZoneTerms.value.startsWith('/intl/en/play-terms')) {
      return true;
    }
    const matchByLangZone =
        '/intl/' + termsLang + '_' + countryCode + '/play-terms';
    if (selectLangZoneTerms.value.startsWith(matchByLangZone)) {
      // Already selected what is needed.
      return true;
    }
    for (let i = selectLangZoneTerms.options.length - 1; i >= 0; --i) {
      const option = selectLangZoneTerms.options[i];
      if (option.value.startsWith(matchByLangZone)) {
        window.location.href = option.value;
        return true;
      }
    }
    return false;
  };

  // Try two versions of the language, full and short (if it exists, for
  // example en-GB -> en). Note, terms may contain entries for both types, for
  // example: en_ie, es-419_ar, es_as, pt-PT_pt.
  if (applyTermsForLangAndZone(language)) {
    return true;
  }
  const langSegments = language.split('-');
  if (langSegments.length === 2 && applyTermsForLangAndZone(langSegments[0])) {
    return true;
  }

  return false;
}

/**
 * Processes select tag that contains list of available terms for different
 * languages and zones. In case of initial load, tries to find terms that match
 * exactly current language and country code and automatically redirects the
 * view in case such terms are found. Leaves terms in select tag that only match
 * current language or country code or default English variant or currently
 * selected.
 *
 * @return {boolean} True.
 */
function processLangZoneTerms(initialLoad, language, countryCode) {
  const langSegments = language.split('-');
  if (initialLoad && navigateToLanguageAndCountryCode(language, countryCode)) {
    document.body.hidden = false;
    return true;
  }

  const footer = getPlayFooterElement();
  if (!footer) {
    // Layout is not recognized, show content and stop processing.
    document.body.hidden = false;
    return true;
  }

  const matchByLang = '/intl/' + language + '_';
  let matchByLangShort = null;
  if (langSegments.length === 2) {
    matchByLangShort = '/intl/' + langSegments[0] + '_';
  }

  const matchByZone = '_' + countryCode + '/play-terms';
  const matchByDefault = '/intl/en/play-terms';

  // We are allowed to display terms by default only in language that matches
  // current UI language. In other cases we have to switch to default version.
  let langMatch = false;
  let defaultExist = false;

  const selectLangZoneTerms = getLangZoneSelect();
  if (!selectLangZoneTerms) {
    document.body.hidden = false;
    return;
  }

  for (let i = selectLangZoneTerms.options.length - 1; i >= 0; --i) {
    const option = selectLangZoneTerms.options[i];
    if (selectLangZoneTerms.selectedIndex === i) {
      langMatch = option.value.startsWith(matchByLang) ||
          (matchByLangShort && option.value.startsWith(matchByLangShort));
      continue;
    }
    if (option.value.startsWith(matchByDefault)) {
      defaultExist = true;
      continue;
    }

    option.hidden = !option.value.startsWith(matchByLang) &&
        !option.value.includes(matchByZone) &&
        !(matchByLangShort && option.value.startsWith(matchByLangShort)) &&
        option.text !== 'English';
  }

  if (initialLoad && !langMatch && defaultExist) {
    window.location.href = matchByDefault;
  } else {
    // Show content once we reached target url.
    document.body.hidden = false;
  }
  return true;
}

/**
 * Returns the raw body HTML of the ToS contents.
 * @return {string} HTML of document body.
 */
function getToSContent() {
  return document.body.innerHTML;
}

/**
 * Formats current document in order to display it correctly.
 */
function formatDocument() {
  if (document.viewMode) {
    document.body.classList.add(document.viewMode);
  }
  // playstore.css is injected into the document and it is applied first.
  // Need to remove existing links that contain references to external
  // stylesheets which override playstore.css.
  const links = document.head.getElementsByTagName('link');
  for (let i = links.length - 1; i >= 0; --i) {
    document.head.removeChild(links[i]);
  }

  // Create base element that forces internal links to be opened in new window.
  const base = document.createElement('base');
  base.target = '_blank';
  document.head.appendChild(base);

  // Hide content at this point. We might want to redirect our view to terms
  // that exactly match current language and country code.
  document.body.hidden = true;
}

/**
 * Searches in footer for a privacy policy link.
 * @return {string} Link to Google Privacy Policy detected from the current
 *                  document or link to the default policy if it is not found.
 */
function getPrivacyPolicyLink() {
  const footer = getPlayFooterElement();
  if (footer) {
    const links = footer.getElementsByTagName('a');
    for (let i = 0; i < links.length; ++i) {
      const targetURL = links[i].href;
      if (targetURL.endsWith('/policies/privacy/')) {
        return targetURL;
      }
    }
  }
  return 'https://www.google.com/policies/privacy/';
}

/**
 * Processes the current document by applying required formatting and selected
 * right PlayStore ToS.
 * Note that document.countryCode must be set before calling this function.
 */
function processDocument() {
  if (document.wasProcessed) {
    return;
  }
  formatDocument();

  const initialLoad =
      window.location.href.startsWith('https://play.google/play-terms');
  let language = document.language;
  if (!language) {
    language = navigator.language;
  }

  processLangZoneTerms(initialLoad, language, document.countryCode);
  document.wasProcessed = true;
}

processDocument();
