/**
 * @license
 * Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @fileoverview The local InstantExtended NTP.
 */

// Global local statics (visible for testing).

/**
 * Whether the Most Visited and edit custom link iframes should be created while
 * running tests. Currently the SimpleJavascriptTests are flaky due to some
 * raciness in the creation/destruction of the iframe. crbug.com/786313.
 * @type {boolean}
 */
let iframesAndVoiceSearchDisabledForTesting = false;

/**
 * Whether the most visited tiles have finished loading, i.e. we've received the
 * 'loaded' postMessage from the iframe. Used by tests to detect that loading
 * has completed.
 * @type {boolean}
 */
let tilesAreLoaded = false;

/**
 * Controls rendering the new tab page for InstantExtended.
 * @return {Object} A limited interface for testing the local NTP.
 */
function LocalNTP() {
'use strict';

// Type definitions.

/** @enum {number} */
const ACMatchClassificationStyle = {
  NONE: 0,
  URL: 1 << 0,
  MATCH: 1 << 1,
  DIM: 1 << 2,
};

/** @typedef {{inline: string, text: string}} */
let RealboxOutput;

/**
 * @typedef {{
 *   moveCursorToEnd: (boolean|undefined),
 *   inline: (string|undefined),
 *   text: (string|undefined),
 * }}
 */
let RealboxOutputUpdate;

// Constants.

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
const CLASSES = {
  ALTERNATE_LOGO: 'alternate-logo',  // Shows white logo if required by theme
  COLLAPSED: 'collapsed',
  // Applies styles to dialogs used in customization.
  CUSTOMIZE_DIALOG: 'customize-dialog',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  DESCRIPTION: 'description',
  DIM: 'dim',
  DISMISSABLE: 'dismissable',
  DISMISS_ICON: 'dismiss-icon',
  DISMISS_PROMO: 'dismiss-promo',
  // Extended and elevated style for customization entry point.
  ENTRY_POINT_ENHANCED: 'ep-enhanced',
  FAKEBOX_FOCUS: 'fakebox-focused',  // Applies focus styles to the fakebox
  // Applies float animations to the Most Visited notification
  FLOAT_DOWN: 'float-down',
  FLOAT_UP: 'float-up',
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  HAS_IMAGE: 'has-image',  // A realbox match with an image.
  // Applies a different style to the error notification if a link is present.
  HAS_LINK: 'has-link',
  HEADER: 'header',
  HIDE_FAKEBOX: 'hide-fakebox',
  HIDE_NOTIFICATION: 'notice-hide',
  // Contains the image next to a realbox match. Displays a placeholder color
  // while the realbox match image is loading.
  IMAGE_CONTAINER: 'image-container',
  INITED: 'inited',  // Reveals the <body> once init() is done.
  LEFT_ALIGN_ATTRIBUTION: 'left-align-attribution',
  // The icon next to a realbox match.
  MATCH_ICON: 'match-icon',
  // The image next to a realbox match.
  MATCH_IMAGE: 'match-image',
  // Vertically centers the most visited section for a non-Google provided page.
  NON_GOOGLE_PAGE: 'non-google-page',
  REMOVABLE: 'removable',
  REMOVE_ICON: 'remove-icon',
  REMOVE_MATCH: 'remove-match',
  SELECTED: 'selected',  // A selected (via up/down arrow key) realbox match.
  SHOW_ELEMENT: 'show-element',
  // When the realbox has matches to show.
  SHOW_MATCHES: 'show-matches',
  // Applied when the doodle shouldn't be shown, e.g. when a theme is applied.
  DONT_SHOW_DOODLE: 'dont-show-doodle',
};

const DOCUMENT_MATCH_TYPE = 'document';

/**
 * The period of time (ms) before transitions can be applied to a toast
 * notification after modifying the "display" property.
 * @type {number}
 */
const DISPLAY_TIMEOUT = 20;

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
const IDS = {
  ATTRIBUTION: 'attribution',
  ATTRIBUTION_TEXT: 'attribution-text',
  CUSTOM_BG: 'custom-bg',
  CUSTOM_LINKS_EDIT_IFRAME: 'custom-links-edit',
  CUSTOM_LINKS_EDIT_IFRAME_DIALOG: 'custom-links-edit-dialog',
  ERROR_NOTIFICATION: 'error-notice',
  ERROR_NOTIFICATION_CONTAINER: 'error-notice-container',
  ERROR_NOTIFICATION_LINK: 'error-notice-link',
  ERROR_NOTIFICATION_MSG: 'error-notice-msg',
  FAKEBOX: 'fakebox',
  FAKEBOX_INPUT: 'fakebox-input',
  FAKEBOX_TEXT: 'fakebox-text',
  FAKEBOX_MICROPHONE: 'fakebox-microphone',
  MOST_VISITED: 'most-visited',
  NOTIFICATION: 'mv-notice',
  NOTIFICATION_CONTAINER: 'mv-notice-container',
  NOTIFICATION_MESSAGE: 'mv-msg',
  NTP_CONTENTS: 'ntp-contents',
  OGB: 'one-google',
  PROMO: 'promo',
  REALBOX: 'realbox',
  REALBOX_ICON: 'realbox-icon',
  REALBOX_INPUT_WRAPPER: 'realbox-input-wrapper',
  REALBOX_MATCHES: 'realbox-matches',
  REALBOX_MICROPHONE: 'realbox-microphone',
  RESTORE_ALL_LINK: 'mv-restore',
  SUGGESTIONS: 'suggestions',
  TILES: 'mv-tiles',
  TILES_IFRAME: 'mv-single',
  UNDO_LINK: 'mv-undo',
  USER_CONTENT: 'user-content',
};

/**
 * The different types of events that are logged from the NTP. This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
const LOG_TYPE = {
  // The One Google Bar was shown.
  NTP_ONE_GOOGLE_BAR_SHOWN: 37,

  // 'Cancel' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_CANCEL: 54,
  // 'Done' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_DONE: 55,

  // A middle slot promo was shown.
  NTP_MIDDLE_SLOT_PROMO_SHOWN: 60,
  // A promo link was clicked.
  NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED: 61,
};

/**
 * The maximum number of tiles to show in the Most Visited section if custom
 * links is enabled.
 * @type {number}
 * @const
 */
const MAX_NUM_TILES_CUSTOM_LINKS = 10;

/**
 * The maximum number of tiles to show in the Most Visited section.
 * @type {number}
 * @const
 */
const MAX_NUM_TILES_MOST_VISITED = 8;

/**
 * Indicates a missing suggestion group Id. Based on
 * SearchSuggestionParser::kNoSuggestionGroupId.
 * @type {number}
 */
const NO_SUGGESTION_GROUP_ID = -1;

/**
 * The period of time (ms) before the Most Visited notification is hidden.
 * @type {number}
 */
const NOTIFICATION_TIMEOUT = 10000;

/**
 * Specifications for an NTP design (not comprehensive).
 *
 * backgroundColor: The 4-component color of default background,
 * darkBackgroundColor: The 4-component color of default dark background,
 * iconBackgroundColor: The 4-component color of default dark icon background,
 * iconDarkBackgroundColor: The 4-component color of default icon background,
 * numTitleLines: Number of lines to display in titles.
 * titleColor: The 4-component color of title text.
 * titleColorAgainstDark: The 4-component color of title text against a dark
 *   theme.
 *
 * @type {{
 *   backgroundColor: !Array<number>,
 *   darkBackgroundColor: !Array<number>,
 *   iconBackgroundColor: !Array<number>,
 *   iconDarkBackgroundColor: !Array<number>,
 *   numTitleLines: number,
 *   titleColor: !Array<number>,
 *   titleColorAgainstDark: !Array<number>,
 * }}
 */
const NTP_DESIGN = {
  backgroundColor: [255, 255, 255, 255],
  darkBackgroundColor: [53, 54, 58, 255],
  iconBackgroundColor: [241, 243, 244, 255],  /** GG100 */
  iconDarkBackgroundColor: [32, 33, 36, 255], /** GG900 */
  numTitleLines: 1,
  titleColor: [60, 64, 67, 255],               /** GG800 */
  titleColorAgainstDark: [248, 249, 250, 255], /** GG050 */
};

const REALBOX_KEYDOWN_HANDLED_KEYS = [
  'ArrowDown',
  'ArrowUp',
  'Delete',
  'Enter',
  'Escape',
  'PageDown',
  'PageUp',
];

// Local statics.

/** @type {?AutocompleteResult} */
let autocompleteResult = null;

/**
 * The time of the first character insert operation that has not yet been
 * painted in floating point milliseconds. Used to measure the realbox
 * responsiveness with a histogram.
 * @type {number}
 */
let charTypedTime = 0;

/**
 * The currently visible notification element. Null if no notification is
 * present.
 * @type {?Object}
 */
let currNotification = null;

/**
 * The timeout function for automatically hiding the pop-up notification. Only
 * set if a notification is visible.
 * @type {?Object}
 */
let delayedHideNotification = null;

/**
 * Whether 'Enter' was pressed but did not navigate to a match due to matches
 * being stale.
 * @type {boolean}
 */
let enterWasPressed = false;

/**
 * A cache from image URL to image content in the form of data URL in order to
 * reuse match image data that have been loaded before and to avoid flickering.
 * @type {!Object<string>}
 */
const faviconOrImageUrlToDataUrlCache = {};

/**
 * True if dark mode is enabled.
 * @type {boolean}
 */
let isDarkModeEnabled = false;

/**
 * Used to prevent the default match from requiring inline autocompletion when
 * the user is deleting text in the input.
 */
let isDeletingInput = false;

/**
 * The last blacklisted tile rid if any, which by definition should not be
 * filler.
 * @type {?number}
 */
let lastBlacklistedTile = null;

/**
 * The 'Enter' event that was ignored due to matches being stale. Will be used
 * to navigate to the default match once up-to-date matches arrive.
 * @type {?Event}
 */
let lastEnterEvent = null;

/**
 * The last queried input.
 * @type {string|undefined}
 */
let lastQueriedInput;

/**
 * Last text/inline autocompletion shown in the realbox (either by user input or
 * outputting autocomplete matches).
 * @type {!RealboxOutput}
 */
let lastOutput = {text: '', inline: ''};

/** @type {?number} */
let lastRealboxFocusTime = null;

/**
 * Current realbox match elements.
 * @type {!Array<!Element>}
 */
let matchEls = [];

/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
let ntpApiHandle;

/**
 * True if user just pasted into the realbox.
 * @type {boolean}
 */
let pastedInRealbox = false;

/**
 * A map from a suggestion Group ID to the group element for that group ID.
 * @type {!Object<!Element>}
 */
let suggestionGroupElsMap = {};

// Helper methods.

/** @return {boolean} */
function areRealboxMatchesVisible() {
  return $(IDS.REALBOX_INPUT_WRAPPER).classList.contains(CLASSES.SHOW_MATCHES);
}

/** @param {!AutocompleteResult} result */
function autocompleteResultChanged(result) {
  if (lastQueriedInput === undefined ||
      result.input !== lastQueriedInput.trimLeft()) {
    return;  // Stale result; ignore.
  }

  renderAutocompleteMatches(result.matches, result.suggestionGroupsMap);

  autocompleteResult = result;

  $(IDS.REALBOX).focus();

  updateRealboxOutput({
    inline: '',
    text: lastQueriedInput || '',
  });

  assert(autocompleteResult.matches.length === matchEls.length);
  const first = result.matches[0];
  if (first && first.allowedToBeDefaultMatch) {
    selectMatchEl(matchEls[0]);
    updateRealboxOutput({inline: first.inlineAutocompletion});

    if (enterWasPressed) {
      assert(lastEnterEvent);
      navigateToMatch(first, lastEnterEvent);
    }
  } else {
    setRealboxIcon(undefined);
  }
}

/**
 * @param {number} matchIndex
 * @param {string} url AutocompleteMatch's imageUrl or destinationUrl.
 * @param {string} dataUrl
 */
function autocompleteMatchImageAvailable(matchIndex, url, dataUrl) {
  if (!autocompleteResult || !autocompleteResult.matches[matchIndex]) {
    return;
  }

  const match = autocompleteResult.matches[matchIndex];
  if (match.imageUrl !== url && match.destinationUrl !== url) {
    return;
  }

  // Return if the image has been rendered. Re-rendering it will cause flicker.
  if (faviconOrImageUrlToDataUrlCache[url]) {
    return;
  }
  faviconOrImageUrlToDataUrlCache[url] = dataUrl;

  assert(autocompleteResult.matches.length === matchEls.length);

  // Update the match image/favicon.
  if (match.imageUrl === url) {
    const imageContainerEl = assert(matchEls[matchIndex].getElementsByClassName(
        CLASSES.IMAGE_CONTAINER)[0]);
    const imageEl = document.createElement('img');
    imageEl.classList.add(CLASSES.MATCH_IMAGE);
    imageEl.src = dataUrl;
    imageContainerEl.appendChild(imageEl);
    imageContainerEl.style.backgroundColor = 'transparent';
  } else {
    const iconEl = assert(
        matchEls[matchIndex].getElementsByClassName(CLASSES.MATCH_ICON)[0]);
    setBackgroundImageByUrl(iconEl, dataUrl);
  }

  // If the match is selected, also update the realbox favicon.
  const selectedMatchIndex = matchEls.findIndex(matchEl => {
    return matchEl.classList.contains(CLASSES.SELECTED);
  });
  if (selectedMatchIndex === matchIndex) {
    setRealboxIcon(match);
  }
}

/**
 * @param {number} style
 * @return {!Array<string>}
 */
function classificationStyleToClasses(style) {
  const classes = [];
  if (style & ACMatchClassificationStyle.DIM) {
    classes.push('dim');
  }
  if (style & ACMatchClassificationStyle.MATCH) {
    classes.push('match');
  }
  if (style & ACMatchClassificationStyle.URL) {
    classes.push('url');
  }
  return classes;
}

function clearAutocompleteMatches() {
  autocompleteResult = null;
  window.chrome.embeddedSearch.searchBox.stopAutocomplete(
      /*clearResult=*/ true);
  // Autocomplete sends updates once it is stopped. Invalidate those results
  // by setting the last queried input to its uninitialized value.
  lastQueriedInput = undefined;
  setRealboxIcon(undefined);
}

/**
 * Converts an Array of color components into RGBA format "rgba(R,G,B,A)".
 * @param {Array<number>} color Array of rgba color components.
 * @return {string} CSS color in RGBA format.
 */
function convertToRGBAColor(color) {
  return 'rgba(' + color[0] + ',' + color[1] + ',' + color[2] + ',' +
      color[3] / 255 + ')';
}

/**
 * Converts an Array of color components into 8-digit Hex format "#RRGGBBAA".
 * @param {Array<number>} color Array of rgba color components.
 * @return {string} CSS color in 8-digit Hex format.
 */
function convertToHexColor(color) {
  return '#' + assert(color).map(c => c.toString(16).padStart(2, '0')).join('');
}

/**
 * Returns a timeout that can be executed early. Calls back true if this was
 * an early execution, false otherwise.
 * @param {!Function} timeout The timeout function. Requires a boolean param.
 * @param {number} delay The timeout delay.
 * @return {Object}
 */
function createExecutableTimeout(timeout, delay) {
  const timeoutId = window.setTimeout(() => {
    timeout(/*executedEarly=*/ false);
  }, delay);
  return {
    clear: () => {
      window.clearTimeout(timeoutId);
    },
    trigger: () => {
      window.clearTimeout(timeoutId);
      return timeout(/*executedEarly=*/ true);
    }
  };
}

/** Create the Most Visited and edit custom links iframes. */
function createIframes() {
  // Collect arguments for the most visited iframe.
  const args = [];

  const searchboxApiHandle = window.chrome.embeddedSearch.searchBox;

  if (searchboxApiHandle.rtl) {
    args.push('rtl=1');
  }
  if (NTP_DESIGN.numTitleLines > 1) {
    args.push('ntl=' + NTP_DESIGN.numTitleLines);
  }

  args.push(
      'title=' +
      encodeURIComponent(configData.translatedStrings.mostVisitedTitle));
  args.push(
      'removeTooltip=' +
      encodeURIComponent(configData.translatedStrings.removeThumbnailTooltip));

  if (configData.isGooglePage) {
    args.push('enableCustomLinks=1');
    args.push(
        'addLink=' +
        encodeURIComponent(configData.translatedStrings.addLinkTitle));
    args.push(
        'addLinkTooltip=' +
        encodeURIComponent(configData.translatedStrings.addLinkTooltip));
    args.push(
        'editLinkTooltip=' +
        encodeURIComponent(configData.translatedStrings.editLinkTooltip));
  }

  // Create the most visited iframe.
  const iframe = document.createElement('iframe');
  iframe.id = IDS.TILES_IFRAME;
  iframe.name = IDS.TILES_IFRAME;
  iframe.title = configData.translatedStrings.mostVisitedTitle;
  iframe.src = 'chrome-search://most-visited/single.html?' + args.join('&');
  $(IDS.TILES).appendChild(iframe);

  iframe.onload = function() {
    sendNtpThemeToMostVisitedIframe();
    reloadTiles();
  };

  if (configData.isGooglePage) {
    // Collect arguments for the edit custom link iframe.
    const clArgs = [];

    if (searchboxApiHandle.rtl) {
      clArgs.push('rtl=1');
    }

    clArgs.push(
        'addTitle=' +
        encodeURIComponent(configData.translatedStrings.addLinkTitle));
    clArgs.push(
        'editTitle=' +
        encodeURIComponent(configData.translatedStrings.editLinkTitle));
    clArgs.push(
        'nameField=' +
        encodeURIComponent(configData.translatedStrings.nameField));
    clArgs.push(
        'urlField=' +
        encodeURIComponent(configData.translatedStrings.urlField));
    clArgs.push(
        'linkRemove=' +
        encodeURIComponent(configData.translatedStrings.linkRemove));
    clArgs.push(
        'linkCancel=' +
        encodeURIComponent(configData.translatedStrings.linkCancel));
    clArgs.push(
        'linkDone=' +
        encodeURIComponent(configData.translatedStrings.linkDone));
    clArgs.push(
        'invalidUrl=' +
        encodeURIComponent(configData.translatedStrings.invalidUrl));

    // Create the edit custom link iframe.
    const clIframe = document.createElement('iframe');
    clIframe.id = IDS.CUSTOM_LINKS_EDIT_IFRAME;
    clIframe.name = IDS.CUSTOM_LINKS_EDIT_IFRAME;
    clIframe.title = configData.translatedStrings.editLinkTitle;
    clIframe.src = 'chrome-search://most-visited/edit.html?' + clArgs.join('&');
    const clIframeDialog = document.createElement('dialog');
    clIframeDialog.id = IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG;
    clIframeDialog.classList.add(CLASSES.CUSTOMIZE_DIALOG);
    clIframeDialog.appendChild(clIframe);
    document.body.appendChild(clIframeDialog);
  }

  window.addEventListener('message', handlePostMessage);
}

/**
 * Return true if custom links are enabled.
 * @return {boolean}
 */
function customLinksEnabled() {
  return configData.isGooglePage &&
      !chrome.embeddedSearch.newTabPage.isUsingMostVisited;
}

/**
 * Called by tests to disable the creation of Most Visited and edit custom link
 * iframes.
 */
function disableIframesAndVoiceSearchForTesting() {
  iframesAndVoiceSearchDisabledForTesting = true;
}

/**
 * TODO(dbeam): reconcile this with //ui/webui/resources/js/util.js.
 * @param {?Node} node The node to check.
 * @param {function(?Node):boolean} predicate The function that tests the
 *     nodes.
 * @return {?Node} The found ancestor or null if not found.
 */
function findAncestor(node, predicate) {
  while (node !== null) {
    if (predicate(node)) {
      break;
    }
    node = node.parentNode;
  }
  return node;
}

/**
 * Animates the pop-up notification to float down, and clears the timeout to
 * hide the notification.
 * @param {?Element} notification The notification element.
 * @param {?Element} notificationContainer The notification container element.
 * @param {boolean} showPromo Do show the promo if present.
 */
function floatDownNotification(notification, notificationContainer, showPromo) {
  if (!notification || !notificationContainer) {
    return;
  }

  if (!notificationContainer.classList.contains(CLASSES.FLOAT_UP)) {
    return;
  }

  // Clear the timeout to hide the notification.
  if (delayedHideNotification) {
    delayedHideNotification.clear();
    delayedHideNotification = null;
    currNotification = null;
  }

  if (showPromo) {
    // Show middle-slot promo if one is present.
    const promo = $(IDS.PROMO);
    if (promo) {
      promo.classList.remove(CLASSES.HIDE_NOTIFICATION);
      // Timeout is required for the "float" transition to work. Modifying the
      // "display" property prevents transitions from activating for a brief
      // period of time.
      window.setTimeout(() => {
        promo.classList.remove(CLASSES.FLOAT_DOWN);
      }, DISPLAY_TIMEOUT);
    }
  }

  // Reset notification visibility once the animation is complete.
  notificationContainer.addEventListener('transitionend', (event) => {
    // Blur the hidden items.
    $(IDS.UNDO_LINK).blur();
    $(IDS.RESTORE_ALL_LINK).blur();
    if (notification.classList.contains(CLASSES.HAS_LINK)) {
      notification.classList.remove(CLASSES.HAS_LINK);
      $(IDS.ERROR_NOTIFICATION_LINK).blur();
    }
    // Hide the notification
    if (!notification.classList.contains(CLASSES.FLOAT_UP)) {
      notification.classList.add(CLASSES.HIDE_NOTIFICATION);
    }
  }, {once: true});
  notificationContainer.classList.remove(CLASSES.FLOAT_UP);
}

/**
 * Animates the specified notification to float up. Automatically hides any
 * pre-existing notification and sets a delayed timer to hide the new
 * notification.
 * @param {?Element} notification The notification element.
 * @param {?Element} notificationContainer The notification container element.
 */
function floatUpNotification(notification, notificationContainer) {
  if (!notification || !notificationContainer) {
    return;
  }

  // Hide any pre-existing notification.
  if (delayedHideNotification) {
    // Hide the current notification if it's a different type (i.e. error vs
    // success). Otherwise, simply clear the notification timeout and reset it
    // later.
    if (currNotification === notificationContainer) {
      delayedHideNotification.clear();
    } else {
      delayedHideNotification.trigger();
    }
    delayedHideNotification = null;
  }

  // Hide middle-slot promo if one is present.
  const promo = $(IDS.PROMO);
  if (promo) {
    promo.classList.add(CLASSES.FLOAT_DOWN);
    // Prevent keyboard focus once the promo is hidden.
    promo.addEventListener('transitionend', (event) => {
      if (event.propertyName === 'bottom' &&
          promo.classList.contains(CLASSES.FLOAT_DOWN)) {
        promo.classList.add(CLASSES.HIDE_NOTIFICATION);
      }
    }, {once: true});
  }

  notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
  // Timeout is required for the "float" transition to work. Modifying the
  // "display" property prevents transitions from activating for a brief period
  // of time.
  window.setTimeout(() => {
    notificationContainer.classList.add(CLASSES.FLOAT_UP);
  }, DISPLAY_TIMEOUT);

  // Automatically hide the notification after a period of time.
  delayedHideNotification = createExecutableTimeout((executedEarly) => {
    // Early execution occurs if another notification should be shown. In this
    // case, we do not want to re-show the promo yet.
    floatDownNotification(notification, notificationContainer, !executedEarly);
  }, NOTIFICATION_TIMEOUT);
  currNotification = notificationContainer;
}

/**
 * Returns theme background info.
 * @return {?NtpTheme}
 */
function getNtpTheme() {
  const info = window.chrome.embeddedSearch.newTabPage.ntpTheme;
  const preview = $(customize.IDS.CUSTOM_BG_PREVIEW);
  if (preview.dataset.hasPreview === 'true') {
    info.isNtpBackgroundDark = preview.dataset.hasImage === 'true';
    info.customBackgroundConfigured = preview.dataset.hasImage === 'true';
    info.alternateLogo = preview.dataset.hasImage === 'true';
    // backgroundImage is in the form: url("actual url"). Remove everything
    // except the actual url.
    info.imageUrl = preview.style.backgroundImage.slice(5, -2);

    if (preview.dataset.hasImage === 'true') {
      info.attribution1 = preview.dataset.attributionLine1;
      info.attribution2 = preview.dataset.attributionLine2;
      info.attributionActionUrl = preview.dataset.attributionActionUrl;
    }
  }
  return info;
}

/**
 * Determine whether dark chips should be used if dark mode is enabled. This is
 * is the case when dark mode is enabled and a background image (from a custom
 * background or user theme) is not set.
 *
 * @param {!Object} info Theme background information.
 * @return {boolean} Whether the chips should be dark.
 */
function getUseDarkChips(info) {
  return window.matchMedia('(prefers-color-scheme: dark)').matches &&
      !info.imageUrl;
}

/**
 * Event handler for messages from the most visited and edit custom link iframe.
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  const cmd = event.data.cmd;
  const args = event.data;
  if (cmd === 'loaded') {
    tilesAreLoaded = true;
    if (configData.isGooglePage) {
      // Show search suggestions, promo, and the OGB if they were previously
      // hidden.
      if ($(IDS.SUGGESTIONS)) {
        $(IDS.SUGGESTIONS).style.visibility = 'visible';
      }
      if ($(IDS.PROMO)) {
        showPromoIfNotOverlappingAndTrackResizes();
      }
      if (customLinksEnabled()) {
        $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT)
            .classList.toggle(
                customize.CLASSES.OPTION_DISABLED, !args.showRestoreDefault);
        $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).tabIndex =
            (args.showRestoreDefault ? 0 : -1);
      }
      $(IDS.OGB).classList.add(CLASSES.SHOW_ELEMENT);
    }
  } else if (cmd === 'tileBlacklisted') {
    if (customLinksEnabled()) {
      showNotification(configData.translatedStrings.linkRemovedMsg);
    } else {
      showNotification(
          configData.translatedStrings.thumbnailRemovedNotification);
    }
    lastBlacklistedTile = args.rid;

    ntpApiHandle.deleteMostVisitedItem(args.rid);
  } else if (cmd === 'resizeDoodle') {
    doodles.resizeDoodleHandler(args);
  } else if (cmd === 'startEditLink') {
    $(IDS.CUSTOM_LINKS_EDIT_IFRAME)
        .contentWindow.postMessage({cmd: 'linkData', rid: args.rid}, '*');
    // Small delay to allow the dialog to finish setting up before displaying.
    window.setTimeout(function() {
      $(IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG).showModal();
    }, 10);
  } else if (cmd === 'closeDialog') {
    $(IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG).close();
  } else if (cmd === 'focusMenu') {
    // Focus the edited tile's menu or the add shortcut tile after closing the
    // custom link edit dialog without saving.
    const iframe = $(IDS.TILES_IFRAME);
    if (iframe) {
      iframe.contentWindow.postMessage({cmd: 'focusMenu', rid: args.rid}, '*');
    }
  }
}

/** Hides the Most Visited pop-up notification. */
function hideNotification() {
  floatDownNotification(
      $(IDS.NOTIFICATION), $(IDS.NOTIFICATION_CONTAINER), /*showPromo=*/ true);
}

/**
 * Prepares the New Tab Page by adding listeners, the most visited pages
 * section, and Google-specific elements for a Google-provided page.
 */
function init() {
  // If an accessibility tool is in use, increase the time for which the
  // "tile was blacklisted" notification is shown.
  if (configData.isAccessibleBrowser) {
    document.body.style.setProperty('--mv-notice-time', '30s');
  }

  // Hide notifications after fade out, so we can't focus on links via keyboard.
  $(IDS.NOTIFICATION).addEventListener('transitionend', (event) => {
    if (event.propertyName === 'opacity') {
      hideNotification();
    }
  });

  $(IDS.NOTIFICATION_MESSAGE).textContent =
      configData.translatedStrings.thumbnailRemovedNotification;

  const undoLink = $(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  registerKeyHandler(undoLink, ['Enter', ' '], onUndo);
  undoLink.textContent = configData.translatedStrings.undoThumbnailRemove;

  const restoreAllLink = $(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  registerKeyHandler(restoreAllLink, ['Enter', ' '], onRestoreAll);

  $(IDS.ATTRIBUTION_TEXT).textContent =
      configData.translatedStrings.attributionIntro;

  const embeddedSearchApiHandle = window.chrome.embeddedSearch;

  ntpApiHandle = embeddedSearchApiHandle.newTabPage;
  ntpApiHandle.onthemechange = onThemeChange;
  ntpApiHandle.onmostvisitedchange = onMostVisitedChange;

  renderTheme();

  window.matchMedia('(prefers-color-scheme: dark)').onchange = onThemeChange;

  const searchboxApiHandle = embeddedSearchApiHandle.searchBox;

  if (configData.isGooglePage) {
    requestAndInsertGoogleResources();
    animations.addRippleAnimations();

    ntpApiHandle.onaddcustomlinkdone = onAddCustomLinkDone;
    ntpApiHandle.onupdatecustomlinkdone = onUpdateCustomLinkDone;
    ntpApiHandle.ondeletecustomlinkdone = onDeleteCustomLinkDone;

    customize.init(showErrorNotification, hideNotification);

    if (configData.realboxEnabled) {
      setRealboxIcon(undefined);

      const realboxEl = $(IDS.REALBOX);
      realboxEl.placeholder = configData.translatedStrings.searchboxPlaceholder;
      // Using .onmousedown instead of addEventListener('mousedown') to support
      // tests.
      realboxEl.onmousedown = onRealboxMouseDown;
      realboxEl.addEventListener('copy', onRealboxCutCopy);
      realboxEl.addEventListener('cut', onRealboxCutCopy);
      realboxEl.addEventListener('focus', onRealboxFocus);
      realboxEl.addEventListener('input', onRealboxInput);
      realboxEl.addEventListener('keyup', onRealboxKeyup);
      realboxEl.addEventListener('paste', onRealboxPaste);

      const realboxWrapper = $(IDS.REALBOX_INPUT_WRAPPER);
      realboxWrapper.addEventListener('focusout', onRealboxWrapperFocusOut);
      realboxWrapper.addEventListener('keydown', onRealboxWrapperKeydown);

      searchboxApiHandle.autocompleteresultchanged = autocompleteResultChanged;
      searchboxApiHandle.autocompletematchimageavailable =
          autocompleteMatchImageAvailable;

      if (!iframesAndVoiceSearchDisabledForTesting) {
        speech.init(
            configData.googleBaseUrl, configData.translatedStrings,
            $(IDS.REALBOX_MICROPHONE), searchboxApiHandle);
      }

      utils.disableOutlineOnMouseClick($(IDS.REALBOX_MICROPHONE));
    } else {
      // Set up the fakebox (which only exists on the Google NTP).
      ntpApiHandle.oninputstart = onInputStart;
      ntpApiHandle.oninputcancel = onInputCancel;

      if (ntpApiHandle.isInputInProgress) {
        onInputStart();
      }

      $(IDS.FAKEBOX_TEXT).textContent =
          configData.translatedStrings.searchboxPlaceholder;

      if (!iframesAndVoiceSearchDisabledForTesting) {
        speech.init(
            configData.googleBaseUrl, configData.translatedStrings,
            $(IDS.FAKEBOX_MICROPHONE), searchboxApiHandle);
      }

      // Listener for updating the key capture state.
      document.body.onmousedown = function(event) {
        if (isFakeboxClick(event)) {
          searchboxApiHandle.startCapturingKeyStrokes();
        } else if (isFakeboxFocused()) {
          searchboxApiHandle.stopCapturingKeyStrokes();
        }
      };
      searchboxApiHandle.onkeycapturechange = function() {
        setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
      };
      const inputbox = $(IDS.FAKEBOX_INPUT);
      inputbox.onpaste = function(event) {
        event.preventDefault();
        // Send pasted text to Omnibox.
        const text = event.clipboardData.getData('text/plain');
        if (text) {
          searchboxApiHandle.paste(text);
        }
      };
      inputbox.ondrop = function(event) {
        event.preventDefault();
        const text = event.dataTransfer.getData('text/plain');
        if (text) {
          searchboxApiHandle.paste(text);
        }
        setFakeboxDragFocus(false);
      };
      inputbox.ondragenter = function() {
        setFakeboxDragFocus(true);
      };
      inputbox.ondragleave = function() {
        setFakeboxDragFocus(false);
      };
      utils.disableOutlineOnMouseClick($(IDS.FAKEBOX_MICROPHONE));

      // Update the fakebox style to match the current key capturing state.
      setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
      // Also tell the browser that we're capturing, otherwise it's possible
      // that both fakebox and Omnibox have visible focus at the same time, see
      // crbug.com/792850.
      if (searchboxApiHandle.isKeyCaptureEnabled) {
        searchboxApiHandle.startCapturingKeyStrokes();
      }
    }

    doodles.init();
  } else {
    document.body.classList.add(CLASSES.NON_GOOGLE_PAGE);
  }

  if (searchboxApiHandle.rtl) {
    $(IDS.NOTIFICATION).dir = 'rtl';
  }

  if (!iframesAndVoiceSearchDisabledForTesting) {
    createIframes();
  }

  utils.setPlatformClass(document.body);
  utils.disableOutlineOnMouseClick($(customize.IDS.EDIT_BG));
  document.documentElement.classList.add(CLASSES.INITED);
}

/**
 * Injects the One Google Bar into the page. Called asynchronously, so that it
 * doesn't block the main page load.
 */
function injectOneGoogleBar(ogb) {
  if (ogb.barHtml === '') {
    return;
  }

  const inHeadStyle = document.createElement('style');
  inHeadStyle.type = 'text/css';
  inHeadStyle.appendChild(document.createTextNode(ogb.inHeadStyle));
  document.head.appendChild(inHeadStyle);

  const inHeadScript = document.createElement('script');
  inHeadScript.type = 'text/javascript';
  inHeadScript.appendChild(document.createTextNode(ogb.inHeadScript));
  document.head.appendChild(inHeadScript);

  renderOneGoogleBarTheme();

  const ogElem = $('one-google');
  ogElem.innerHTML = ogb.barHtml;

  const afterBarScript = document.createElement('script');
  afterBarScript.type = 'text/javascript';
  afterBarScript.appendChild(document.createTextNode(ogb.afterBarScript));
  ogElem.parentNode.insertBefore(afterBarScript, ogElem.nextSibling);

  $('one-google-end-of-body').innerHTML = ogb.endOfBodyHtml;

  const endOfBodyScript = document.createElement('script');
  endOfBodyScript.type = 'text/javascript';
  endOfBodyScript.appendChild(document.createTextNode(ogb.endOfBodyScript));
  document.body.appendChild(endOfBodyScript);

  ntpApiHandle.logEvent(LOG_TYPE.NTP_ONE_GOOGLE_BAR_SHOWN);
}

/**
 * Injects a middle-slot promo into the page. Called asynchronously, so that it
 * doesn't block the main page load.
 */
function injectPromo(promo) {
  if (!promo.promoHtml) {
    if ($(IDS.PROMO)) {
      $(IDS.PROMO).remove();
    }
    return;
  }

  const promoContainer = document.createElement('div');
  promoContainer.id = IDS.PROMO;
  promoContainer.innerHTML += promo.promoHtml;
  $(IDS.NTP_CONTENTS).appendChild(promoContainer);

  if (promo.promoLogUrl) {
    navigator.sendBeacon(promo.promoLogUrl);
  }

  ntpApiHandle.logEvent(LOG_TYPE.NTP_MIDDLE_SLOT_PROMO_SHOWN);

  const link = promoContainer.querySelector('a');
  if (link) {
    link.onclick = e => {
      const url = new URL(link.href);
      if (promo.canOpenExtensionsPage && url.origin == 'chrome://extensions') {
        ntpApiHandle.openExtensionsPage(
            e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey);
        e.preventDefault();
      }
      ntpApiHandle.logEvent(LOG_TYPE.NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED);
    };
  }

  if (promo.promoId) {
    const icon = document.createElement('button');
    icon.classList.add(CLASSES.DISMISS_ICON);

    icon.title = configData.translatedStrings.dismissPromo;
    icon.onclick = e => {
      ntpApiHandle.blocklistPromo(promo.promoId);
      promoContainer.remove();
      window.removeEventListener('resize', showPromoIfNotOverlapping);
    };

    const dismiss = document.createElement('div');
    dismiss.classList.add(CLASSES.DISMISS_PROMO);
    dismiss.appendChild(icon);

    promoContainer.querySelector('div').appendChild(dismiss);
    promoContainer.classList.add(CLASSES.DISMISSABLE);
  }

  // The the MV tiles are already loaded show the promo immediately.
  if (tilesAreLoaded) {
    showPromoIfNotOverlappingAndTrackResizes();
  }
}

/**
 * Injects search suggestions into the page. Called *synchronously* with cached
 * data as not to cause shifting of the most visited tiles.
 */
function injectSearchSuggestions(suggestions) {
  if (suggestions.suggestionsHtml === '') {
    return;
  }

  const suggestionsContainer = document.createElement('div');
  suggestionsContainer.id = IDS.SUGGESTIONS;
  suggestionsContainer.style.visibility = 'hidden';
  suggestionsContainer.innerHTML += suggestions.suggestionsHtml;
  $(IDS.USER_CONTENT).insertAdjacentElement('afterbegin', suggestionsContainer);

  const endOfBodyScript = document.createElement('script');
  endOfBodyScript.type = 'text/javascript';
  endOfBodyScript.appendChild(
      document.createTextNode(suggestions.suggestionsEndOfBodyScript));
  document.body.appendChild(endOfBodyScript);
}

/**
 * @param {!Event} event The click event.
 * @return {boolean} True if the click occurred in an enabled fakebox.
 */
function isFakeboxClick(event) {
  return $(IDS.FAKEBOX).contains(/** @type HTMLElement */ (event.target)) &&
      !$(IDS.FAKEBOX_MICROPHONE)
           .contains(/** @type HTMLElement */ (event.target));
}

/** @return {boolean} True if the fakebox has focus. */
function isFakeboxFocused() {
  return document.body.classList.contains(CLASSES.FAKEBOX_FOCUS) ||
      document.body.classList.contains(CLASSES.FAKEBOX_DRAG_FOCUS);
}

/** @return {boolean} */
function isPromoOverlapping() {
  const MARGIN = 10;

  /**
   * @param {string} id
   * @return {DOMRect}
   */
  const rect = id => $(id).getBoundingClientRect();

  const promoRect = $(IDS.PROMO).querySelector('div').getBoundingClientRect();

  if (promoRect.top - MARGIN <= rect(IDS.USER_CONTENT).bottom) {
    return true;
  }

  if (window.chrome.embeddedSearch.searchBox.rtl) {
    const attributionRect = rect(IDS.ATTRIBUTION);
    if (attributionRect.width > 0 &&
        promoRect.left - MARGIN <= attributionRect.right) {
      return true;
    }

    const editBgRect = rect(customize.IDS.EDIT_BG);
    assert(editBgRect.width > 0);
    if (promoRect.left - 2 * MARGIN <= editBgRect.right) {
      return true;
    }

    const customAttributionsRect = rect(customize.IDS.ATTRIBUTIONS);
    if (customAttributionsRect.width > 0 &&
        promoRect.right + MARGIN >= customAttributionsRect.left) {
      return true;
    }
  } else {
    const customAttributionsRect = rect(customize.IDS.ATTRIBUTIONS);
    if (customAttributionsRect.width > 0 &&
        promoRect.left - MARGIN <= customAttributionsRect.right) {
      return true;
    }

    const editBgRect = rect(customize.IDS.EDIT_BG);
    assert(editBgRect.width > 0);
    if (promoRect.right + 2 * MARGIN >= editBgRect.left) {
      return true;
    }

    const attributionEl = $(IDS.ATTRIBUTION);
    const attributionRect = attributionEl.getBoundingClientRect();
    if (attributionRect.width > 0) {
      const attributionOnLeft =
          attributionEl.classList.contains(CLASSES.LEFT_ALIGN_ATTRIBUTION);
      if (attributionOnLeft) {
        if (promoRect.left - MARGIN <= attributionRect.right) {
          return true;
        }
      } else if (promoRect.right + MARGIN >= attributionRect.left) {
        return true;
      }
    }
  }

  return false;
}

/** Binds event listeners. */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}

/**
 * @param {!AutocompleteMatch} match
 * @param {!Event} e
 */
function navigateToMatch(match, e) {
  const line = autocompleteResult.matches.indexOf(match);
  assert(line >= 0);
  assert(lastRealboxFocusTime);
  window.chrome.embeddedSearch.searchBox.openAutocompleteMatch(
      line, match.destinationUrl, areRealboxMatchesVisible(),
      Date.now() - lastRealboxFocusTime, e.button || 0, e.altKey, e.ctrlKey,
      e.metaKey, e.shiftKey);
  e.preventDefault();
}

/**
 * Callback for embeddedSearch.newTabPage.onaddcustomlinkdone. Called when the
 * custom link was successfully added. Shows the "Shortcut added" notification.
 * @param {boolean} success True if the link was successfully added.
 */
function onAddCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkAddedMsg);
  } else {
    showErrorNotification(
        configData.translatedStrings.linkCantCreate, null, null);
  }
  ntpApiHandle.logEvent(LOG_TYPE.NTP_CUSTOMIZE_SHORTCUT_DONE);
}

/**
 * Callback for embeddedSearch.newTabPage.ondeletecustomlinkdone. Called when
 * the custom link was successfully deleted. Shows the "Shortcut deleted"
 * notification.
 * @param {boolean} success True if the link was successfully deleted.
 */
function onDeleteCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkRemovedMsg);
  } else {
    showErrorNotification(
        configData.translatedStrings.linkCantRemove, null, null);
  }
}

/**
 * Callback for embeddedSearch.newTabPage.oninputcancel. Restores the NTP
 * (re-enables the fakebox and unhides the logo.)
 */
function onInputCancel() {
  setFakeboxVisibility(true);
}

/**
 * Callback for embeddedSearch.newTabPage.oninputstart. Handles new input by
 * disposing the NTP, according to where the input was entered.
 */
function onInputStart() {
  if (isFakeboxFocused()) {
    setFakeboxFocus(false);
    setFakeboxDragFocus(false);
    setFakeboxVisibility(false);
  }
}

/**
 * Callback for embeddedSearch.newTabPage.onmostvisitedchange. Called when the
 * NTP tiles are updated.
 */
function onMostVisitedChange() {
  reloadTiles();
}

/** @param {Event} e */
function onRealboxMouseDown(e) {
  if (!e.isTrusted || e.button !== 0) {
    // Only handle main (generally left) button presses generated by a user
    // action.
    return;
  }
  if (!$(IDS.REALBOX).value) {
    queryAutocomplete('');
  }
}

/** @param {!Event} e */
function onRealboxCutCopy(e) {
  const realboxEl = $(IDS.REALBOX);
  if (!realboxEl.value || realboxEl.selectionStart !== 0 ||
      realboxEl.selectionEnd !== realboxEl.value.length ||
      !autocompleteResult || autocompleteResult.matches.length === 0) {
    // Only handle cut/copy when realbox has content and it's all selected.
    return;
  }

  const selected = matchEls.findIndex(matchEl => {
    return matchEl.classList.contains(CLASSES.SELECTED);
  });

  const selectedMatch = autocompleteResult.matches[selected];
  if (selectedMatch && !selectedMatch.isSearchType) {
    e.clipboardData.setData('text/plain', selectedMatch.destinationUrl);
    e.preventDefault();
    if (e.type === 'cut') {
      realboxEl.value = '';
    }
  }
}

function onRealboxFocus() {
  lastRealboxFocusTime = Date.now();
}

function onRealboxInput() {
  const realboxValue = $(IDS.REALBOX).value;

  updateRealboxOutput({inline: '', text: realboxValue});

  const charTyped = !isDeletingInput && !!realboxValue.trim();
  // If a character has been typed, update |charTypedTime|. Otherwise reset it.
  // If |charTypedTime| is not 0, there's a pending typed character for which
  // the results have not been painted yet. In that case, keep the earlier time.
  charTypedTime = charTyped ? charTypedTime || window.performance.now() : 0;

  if (realboxValue.trim()) {
    queryAutocomplete(realboxValue);
  } else {
    setRealboxMatchesVisible(false);
    clearAutocompleteMatches();
  }

  pastedInRealbox = false;
}

/** @param {!Event} e */
function onRealboxKeyup(e) {
  if (e.key === 'Tab' && !$(IDS.REALBOX).value) {
    queryAutocomplete('');
  }
}

function onRealboxPaste() {
  pastedInRealbox = true;
}

/** @param {!Event} e */
function onRealboxMatchesFocusIn(e) {
  const target = /** @type {Element} */ (e.target);
  const matchEl = findAncestor(target, el => el.nodeName === 'A');
  if (!matchEl) {
    return;
  }
  const selectedIndex = selectMatchEl(matchEl);
  const selectedMatch = autocompleteResult.matches[selectedIndex];
  if (!selectedMatch) {
    return;
  }

  // It doesn't really make sense to use fillFromMatch() here as the focus
  // change drops the selection (and is probably just noisy to
  // screenreaders).
  updateRealboxOutput(
      {moveCursorToEnd: true, inline: '', text: selectedMatch.fillIntoEdit});
}

/** @param {Event} e */
function onRealboxWrapperFocusOut(e) {
  // Hide the matches and stop autocomplete only when the focus goes outside of
  // the realbox wrapper.
  const relatedTarget = /** @type {Element} */ (e.relatedTarget);
  const realboxWrapper = $(IDS.REALBOX_INPUT_WRAPPER);
  if (!realboxWrapper.contains(relatedTarget)) {
    // Clear the input if it was empty when displaying the matches.
    if (lastQueriedInput === '') {
      updateRealboxOutput({inline: '', text: ''});
      setRealboxIcon(undefined);
    }
    setRealboxMatchesVisible(false);

    // Stop autocomplete but leave (potentially stale) results and continue
    // listening for key presses. These stale results should never be shown, but
    // correspond to the potentially stale suggestion left in the realbox when
    // blurred. That stale result may be navigated to by focusing and pressing
    // Enter.
    window.chrome.embeddedSearch.searchBox.stopAutocomplete(
        /*clearResult=*/ false);
  }
}

/** @param {Event} e */
function onRealboxWrapperKeydown(e) {
  const key = e.key;

  const realboxEl = $(IDS.REALBOX);
  if (e.target === realboxEl && lastOutput.inline) {
    const realboxValue = realboxEl.value;
    const realboxSelected = realboxValue.substring(
        realboxEl.selectionStart, realboxEl.selectionEnd);
    // If the current state matches the default text + inline autocompletion
    // and the user types the next key in the inline autocompletion, just move
    // the selection and requery autocomplete. This is required to avoid flicker
    // while setting .value and .selection{Start,End} to keep typing smooth.
    if (realboxSelected === lastOutput.inline &&
        realboxValue === lastOutput.text + lastOutput.inline &&
        lastOutput.inline[0].toLocaleLowerCase() === key.toLocaleLowerCase()) {
      updateRealboxOutput({
        inline: lastOutput.inline.substr(1),
        text: assert(lastOutput.text + key),
      });

      // If |charTypedTime| is not 0, there's a pending typed character for
      // which the results have not been painted yet. In that case, keep the
      // earlier time.
      charTypedTime = charTypedTime || window.performance.now();

      queryAutocomplete(lastOutput.text);
      e.preventDefault();
      return;
    }
  }

  if (!REALBOX_KEYDOWN_HANDLED_KEYS.includes(key)) {
    return;
  }

  if (!areRealboxMatchesVisible()) {
    if (key === 'ArrowUp' || key === 'ArrowDown') {
      const realboxValue = $(IDS.REALBOX).value;
      if (realboxValue.trim() || !realboxValue) {
        queryAutocomplete(realboxValue);
      }
      e.preventDefault();
      return;
    }
  }

  if (!autocompleteResult || autocompleteResult.matches.length === 0) {
    return;
  }

  assert(autocompleteResult.matches.length === matchEls.length);
  const selected = matchEls.findIndex(matchEl => {
    return matchEl.classList.contains(CLASSES.SELECTED);
  });

  if (key === 'Enter') {
    if (matchEls.concat(realboxEl).includes(e.target)) {
      if (lastQueriedInput === autocompleteResult.input) {
        if (autocompleteResult.matches[selected]) {
          navigateToMatch(autocompleteResult.matches[selected], e);
        }
      } else {
        // User typed and pressed 'Enter' too quickly. Ignore this for now
        // because the matches are stale. Navigate to the default match (if one
        // exists) once the up-to-date results arrive.
        enterWasPressed = true;
        lastEnterEvent = e;
        e.preventDefault();
      }
    }
    return;
  }

  if (key === 'Delete') {
    if (e.shiftKey && !e.altKey && !e.ctrlKey && !e.metaKey) {
      const selectedMatch = autocompleteResult.matches[selected];
      if (selectedMatch && selectedMatch.supportsDeletion) {
        window.chrome.embeddedSearch.searchBox.deleteAutocompleteMatch(
            selected);
        e.preventDefault();
      }
    }
    return;
  }

  if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) {
    return;
  }

  if (key === 'Escape' && selected === 0) {
    updateRealboxOutput({inline: '', text: ''});
    setRealboxMatchesVisible(false);
    clearAutocompleteMatches();
    e.preventDefault();
    return;
  }

  const visibleMatchEls = matchEls.filter((matchEl) => {
    return window.getComputedStyle(matchEl).display !== 'none';
  });
  /** @type {number} */ let newSelected;
  if (key === 'ArrowDown') {
    newSelected = selected + 1 < visibleMatchEls.length ? selected + 1 : 0;
  } else if (key === 'ArrowUp') {
    newSelected = selected - 1 >= 0 ? selected - 1 : visibleMatchEls.length - 1;
  } else if (key === 'Escape' || key === 'PageUp') {
    newSelected = 0;
  } else if (key === 'PageDown') {
    newSelected = visibleMatchEls.length - 1;
  }
  assert(selectMatchEl(assert(visibleMatchEls[newSelected])) >= 0);
  e.preventDefault();

  const realboxMatchesEl = $(IDS.REALBOX_MATCHES);
  if (realboxMatchesEl.contains(document.activeElement)) {
    // Selection should match focus if focus is currently in the matches.
    matchEls[newSelected].focus();
  }

  const newMatch = autocompleteResult.matches[newSelected];
  const newFill = newMatch.fillIntoEdit;
  let newInline = '';
  if (newMatch.allowedToBeDefaultMatch) {
    newInline = newMatch.inlineAutocompletion;
  }
  const newFillEnd = newFill.length - newInline.length;
  updateRealboxOutput({
    moveCursorToEnd: true,
    inline: newInline,
    text: assert(newFill.substr(0, newFillEnd)),
  });
}

/**
 * Handles a click on the restore all notification link by hiding the
 * notification and informing Chrome.
 */
function onRestoreAll() {
  hideNotification();
  // Focus on the omnibox after the notification is hidden.
  window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes();
  if (customLinksEnabled()) {
    ntpApiHandle.resetCustomLinks();
  } else {
    ntpApiHandle.undoAllMostVisitedDeletions();
  }
}

/**
 * Callback for embeddedSearch.newTabPage.onthemechange.
 */
function onThemeChange() {
  renderTheme();
  renderOneGoogleBarTheme();
  sendNtpThemeToMostVisitedIframe();
  if ($(IDS.PROMO)) {
    showPromoIfNotOverlapping();
  }
}

/**
 * Handles a click on the notification undo link by hiding the notification and
 * informing Chrome.
 */
function onUndo() {
  hideNotification();
  // Focus on the omnibox after the notification is hidden.
  window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes();
  if (customLinksEnabled()) {
    ntpApiHandle.undoCustomLinkAction();
  } else if (lastBlacklistedTile != null) {
    ntpApiHandle.undoMostVisitedDeletion(lastBlacklistedTile);
  }
}

/**
 * Callback for embeddedSearch.newTabPage.onupdatecustomlinkdone. Called when
 * the custom link was successfully updated. Shows the "Shortcut edited"
 * notification.
 * @param {boolean} success True if the link was successfully updated.
 */
function onUpdateCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkEditedMsg);
  } else {
    showErrorNotification(
        configData.translatedStrings.linkCantEdit, null, null);
  }
}

/**
 * Called by tests to override the executable timeout with a test timeout.
 * @param {!Function} timeout The timeout function. Requires a boolean param.
 */
function overrideExecutableTimeoutForTesting(timeout) {
  createExecutableTimeout = timeout;
}

/**
 * @param {string} input
 */
function queryAutocomplete(input) {
  lastQueriedInput = input;
  const preventInlineAutocomplete = isDeletingInput || pastedInRealbox ||
      $(IDS.REALBOX).selectionStart !== input.length;  // Caret not at the end.
  window.chrome.embeddedSearch.searchBox.queryAutocomplete(
      input, preventInlineAutocomplete);
}

/**
 * @param {!Element} element
 * @param {!Array<string>} keys
 * @param {!function(Event)} handler
 */
function registerKeyHandler(element, keys, handler) {
  element.addEventListener('keydown', e => {
    if (keys.includes(e.key)) {
      handler(e);
    }
  });
}

/**
 * Fetches new data (RIDs) from the embeddedSearch.newTabPage API and passes
 * them to the iframe.
 */
function reloadTiles() {
  // Don't attempt to load tiles if the MV data isn't available yet - this can
  // happen occasionally, see https://crbug.com/794942. In that case, we should
  // get an onMostVisitedChange call once they are available.
  // Note that MV data being available is different from having > 0 tiles. There
  // can legitimately be 0 tiles, e.g. if the user blacklisted them all.
  if (!ntpApiHandle.mostVisitedAvailable) {
    return;
  }

  const pages = ntpApiHandle.mostVisited;
  const cmds = [];
  const maxNumTiles = customLinksEnabled() ? MAX_NUM_TILES_CUSTOM_LINKS :
                                             MAX_NUM_TILES_MOST_VISITED;
  for (let i = 0; i < Math.min(maxNumTiles, pages.length); ++i) {
    cmds.push({cmd: 'tile', rid: pages[i].rid});
  }
  cmds.push({cmd: 'show'});

  $(IDS.MOST_VISITED).hidden =
      !chrome.embeddedSearch.newTabPage.areShortcutsVisible;

  const iframe = $(IDS.TILES_IFRAME);
  if (iframe) {
    iframe.contentWindow.postMessage(cmds, '*');
  }
}

/**
 * @param {!Array<!AutocompleteMatch>} matches
 * @param {!Object<!SuggestionGroup>} suggestionGroupsMap
 */
function renderAutocompleteMatches(matches, suggestionGroupsMap) {
  const realboxMatchesEl = document.createElement('div');
  realboxMatchesEl.setAttribute('role', 'listbox');

  const newMatchEls = [];
  suggestionGroupElsMap = {};

  /**
   * Creates and returns an action button that once clicked invokes |callback|.
   * @param {!function()} callback
   */
  function createActionButton(callback) {
    const icon = document.createElement('div');
    icon.classList.add(CLASSES.REMOVE_ICON);
    const action = document.createElement('button');
    action.classList.add(CLASSES.REMOVE_MATCH);
    action.appendChild(icon);
    action.onmousedown = e => {
      e.preventDefault();  // Stops default browser action (focus)
    };
    action.onauxclick = e => {
      if (e.button == 1) {
        // Middle click on delete should just noop for now (matches omnibox).
        e.preventDefault();
      }
    };
    action.onclick = e => {
      callback();
      e.preventDefault();  // Stops default browser action (navigation)
    };

    return action;
  }

  /**
   * Creates and returns an element to contain the header as well as the matches
   * belonging to |suggestionGroupId|.
   * @param {number} suggestionGroupId
   */
  function createSuggestionGroupEl(suggestionGroupId) {
    if (suggestionGroupElsMap[suggestionGroupId]) {
      return suggestionGroupElsMap[suggestionGroupId];
    }

    const suggestionGroup = assert(suggestionGroupsMap[suggestionGroupId]);

    /**
     * Updates the tooltip and a11y label of the suggestion group toggle button.
     * @param {!Element} toggleButtonEl
     * @param {boolean} groupIsHidden
     */
    function updateToggleButtonA11y(toggleButtonEl, groupIsHidden) {
      toggleButtonEl.title = groupIsHidden ?
          configData.translatedStrings.showSuggestions :
          configData.translatedStrings.hideSuggestions;
      toggleButtonEl.ariaLabel = utils.substituteString(
          groupIsHidden ? configData.translatedStrings.showSection :
                          configData.translatedStrings.hideSection,
          suggestionGroup.header);
    }

    const groupEl = document.createElement('div');
    groupEl.classList.toggle(CLASSES.COLLAPSED, suggestionGroup.hidden);
    const headerEl = document.createElement('a');
    headerEl.classList.add(CLASSES.HEADER);
    // The header cannot be tabbed into but it will get focus when clicked;
    // preventing the popup from losing focus and closing as a result.
    headerEl.tabIndex = -1;
    headerEl.append(document.createTextNode(suggestionGroup.header));

    const toggle = createActionButton(() => {
      groupEl.classList.toggle(CLASSES.COLLAPSED);
      updateToggleButtonA11y(
          toggle, groupEl.classList.contains(CLASSES.COLLAPSED));
      window.chrome.embeddedSearch.searchBox.toggleSuggestionGroupIdVisibility(
          suggestionGroupId);
    });
    updateToggleButtonA11y(toggle, suggestionGroup.hidden);
    headerEl.appendChild(toggle);
    realboxMatchesEl.classList.add(CLASSES.REMOVABLE);

    groupEl.appendChild(headerEl);
    realboxMatchesEl.appendChild(groupEl);
    suggestionGroupElsMap[suggestionGroupId] = groupEl;
    return groupEl;
  }

  for (let i = 0; i < matches.length; ++i) {
    const match = matches[i];
    const matchEl = document.createElement('a');
    matchEl.href = match.destinationUrl;
    matchEl.setAttribute('role', 'option');

    matchEl.onclick = matchEl.onauxclick = e => {
      if (!e.isTrusted || e.defaultPrevented || e.button > 1) {
        // Don't re-handle events dispatched from navigateToMatch(). Ignore
        // already handled events (i.e. remove button, defaultPrevented). Ignore
        // right clicks (but do handle middle click, button == 1).
        return;
      }
      const target = /** @type {Element} */ (e.target);
      const link = findAncestor(target, el => el.nodeName === 'A');
      if (link === matchEl) {
        navigateToMatch(match, e);
      }
    };

    const hasImage = !!match.imageUrl;
    if (hasImage) {
      matchEl.classList.add(CLASSES.HAS_IMAGE);
    }

    if (hasImage) {
      const imageContainer = document.createElement('div');
      imageContainer.classList.add(CLASSES.IMAGE_CONTAINER);

      if (faviconOrImageUrlToDataUrlCache[match.imageUrl]) {
        const imageEl = document.createElement('img');
        imageEl.classList.add(CLASSES.MATCH_IMAGE);
        imageEl.src = faviconOrImageUrlToDataUrlCache[match.imageUrl];
        imageContainer.appendChild(imageEl);
      } else if (match.imageDominantColor) {
        // .25 Opacity matching c/b/u/views/omnibox/omnibox_match_cell_view.cc
        imageContainer.style.backgroundColor = match.imageDominantColor + '40';
      }
      matchEl.appendChild(imageContainer);
    } else {
      const iconEl = document.createElement('div');
      iconEl.classList.add(CLASSES.MATCH_ICON);
      if (faviconOrImageUrlToDataUrlCache[match.destinationUrl]) {
        setBackgroundImageByUrl(
            iconEl, faviconOrImageUrlToDataUrlCache[match.destinationUrl]);
      } else if (match.type == DOCUMENT_MATCH_TYPE) {
        // Document matches use colored SVG icons.
        setBackgroundImageByUrl(iconEl, match.iconUrl);
      } else {
        setWebkitMaskImageByUrl(iconEl, match.iconUrl);
      }
      matchEl.appendChild(iconEl);
    }

    const contentsEl =
        renderMatchClassifications(match.contents, match.contentsClass);
    let descriptionEl;
    let separatorEl;
    let separatorText = '';

    if (match.description) {
      descriptionEl =
          renderMatchClassifications(match.description, match.descriptionClass);
      descriptionEl.classList.add(CLASSES.DESCRIPTION);
      if (hasImage) {
        descriptionEl.classList.add(CLASSES.DIM);
      } else {
        separatorText = configData.translatedStrings.realboxSeparator;
        separatorEl = document.createTextNode(separatorText);
      }
    }

    const ariaLabel = match.swapContentsAndDescription ?
        match.description + separatorText + match.contents :
        match.contents + separatorText + match.description;
    matchEl.setAttribute('aria-label', ariaLabel);

    const layout = match.swapContentsAndDescription ?
        [descriptionEl, separatorEl, contentsEl] :
        [contentsEl, separatorEl, descriptionEl];

    for (const colEl of layout) {
      if (colEl) {
        matchEl.appendChild(colEl);
      }
    }

    if (match.supportsDeletion) {
      const remove = createActionButton(() => {
        window.chrome.embeddedSearch.searchBox.deleteAutocompleteMatch(i);
      });
      remove.title = configData.translatedStrings.removeSuggestion;
      matchEl.appendChild(remove);
      realboxMatchesEl.classList.add(CLASSES.REMOVABLE);
    }

    if (match.suggestionGroupId &&
        match.suggestionGroupId !== NO_SUGGESTION_GROUP_ID) {
      const groupEl = createSuggestionGroupEl(match.suggestionGroupId);
      groupEl.append(matchEl);
    } else {
      realboxMatchesEl.append(matchEl);
    }
    newMatchEls.push(matchEl);
  }

  if (charTypedTime) {
    window.chrome.embeddedSearch.searchBox.logCharTypedToRepaintLatency(
        Math.floor(window.performance.now() - charTypedTime));
    charTypedTime = 0;
  }

  // When the matches are replaced, the focus gets dropped temporariliy as the
  // focused element is being deleted from the DOM. Stop listening to 'focusout'
  // event and restore it immediately after since we don't want to stop
  // autocomplete in those cases.
  const realboxWrapper = $(IDS.REALBOX_INPUT_WRAPPER);
  realboxWrapper.removeEventListener('focusout', onRealboxWrapperFocusOut);

  $(IDS.REALBOX_MATCHES).remove();
  realboxMatchesEl.id = IDS.REALBOX_MATCHES;
  realboxMatchesEl.addEventListener('focusin', onRealboxMatchesFocusIn);

  realboxWrapper.appendChild(realboxMatchesEl);
  matchEls = newMatchEls;

  realboxWrapper.addEventListener('focusout', onRealboxWrapperFocusOut);

  const hasMatches = matches.length > 0;
  setRealboxMatchesVisible(hasMatches);
}

/**
 * @param {string} text
 * @param {!Array<!ACMatchClassification>} classifications
 * @return {!Element}
 */
function renderMatchClassifications(text, classifications) {
  return classifications
      .map((classification, i) => {
        const classes = classificationStyleToClasses(classification.style);
        const next = classifications[i + 1] || {offset: text.length};
        const classifiedText =
            text.substring(classification.offset, next.offset);
        return classes.length ? spanWithClasses(classifiedText, classes) :
                                document.createTextNode(classifiedText);
      })
      .reduce((container, currentEl) => {
        container.appendChild(currentEl);
        return container;
      }, document.createElement('span'));
}

/**
 * Updates the OneGoogleBar (if it is loaded) based on the current theme.
 * TODO(crbug.com/918582): Add support for OGB dark mode.
 */
function renderOneGoogleBarTheme() {
  if (!window.gbar) {
    return;
  }
  try {
    const oneGoogleBarApi = window.gbar.a;
    const oneGoogleBarPromise = oneGoogleBarApi.bf();
    oneGoogleBarPromise.then(function(oneGoogleBar) {
      const setForegroundStyle = oneGoogleBar.pc.bind(oneGoogleBar);
      const ntpTheme = getNtpTheme();
      setForegroundStyle(ntpTheme && ntpTheme.isNtpBackgroundDark ? 1 : 0);
    });
  } catch (err) {
    console.log('Failed setting OneGoogleBar theme:\n' + err);
  }
}

/** Updates the NTP based on the current theme. */
function renderTheme() {
  const theme = getNtpTheme();
  if (!theme) {
    return;
  }

  // Update dark mode styling.
  isDarkModeEnabled = window.matchMedia('(prefers-color-scheme: dark)').matches;
  document.body.classList.toggle('light-chip', !getUseDarkChips(theme));

  // Dark mode uses a white Google logo.
  const useWhiteLogo =
      theme.alternateLogo || (theme.usingDefaultTheme && isDarkModeEnabled);
  document.body.classList.toggle(CLASSES.ALTERNATE_LOGO, useWhiteLogo);

  if (theme.logoColor) {
    document.body.style.setProperty(
        '--logo-color', convertToRGBAColor(theme.logoColor));
  }

  // The doodle shouldn't be shown for non-default backgrounds. This  includes
  // non-white backgrounds, excluding dark mode gray if dark mode is enabled.
  document.body.classList.toggle(
      CLASSES.DONT_SHOW_DOODLE, !theme.usingDefaultTheme || !!theme.imageUrl);

  // If a custom background has been selected the image will be applied to the
  // custom-background element instead of the body.
  if (!theme.customBackgroundConfigured) {
    document.body.style.background = [
      convertToRGBAColor(theme.backgroundColorRgba), theme.imageUrl,
      theme.imageTiling, theme.imageHorizontalAlignment,
      theme.imageVerticalAlignment
    ].join(' ').trim();

    $(IDS.CUSTOM_BG).style.opacity = '0';
    $(IDS.CUSTOM_BG).style.backgroundImage = '';
    customize.clearAttribution();
  } else {
    // Do anything only if the custom background changed.
    const imageUrl = assert(theme.imageUrl);
    if (!$(IDS.CUSTOM_BG).style.backgroundImage.includes(imageUrl)) {
      const imageWithOverlay = [
        customize.CUSTOM_BACKGROUND_OVERLAY, 'url(' + imageUrl + ')'
      ].join(',').trim();
      // If the theme update is because of uploading a local image then we
      // should close the customization menu. Closing the menu before the image
      // is selected doesn't look good.
      const localImageFileName = 'background.jpg';
      if (!configData.richerPicker &&
          imageWithOverlay.includes(localImageFileName)) {
        customize.closeCustomizationDialog();
      }
      // |image| and |imageWithOverlay| use the same url as their source.
      // Waiting to display the custom background until |image| is fully
      // loaded ensures that |imageWithOverlay| is also loaded.
      $(IDS.CUSTOM_BG).style.backgroundImage = imageWithOverlay;
      const image = new Image();
      image.onload = function() {
        $(IDS.CUSTOM_BG).style.opacity = '1';
      };
      image.src = imageUrl;

      customize.clearAttribution();
      customize.setAttribution(
          '' + theme.attribution1, '' + theme.attribution2,
          '' + theme.attributionActionUrl);
    }
  }

  updateThemeAttribution(theme.attributionUrl, theme.imageHorizontalAlignment);
  setCustomThemeStyle(theme);

  $(customize.IDS.RESTORE_DEFAULT)
      .classList.toggle(
          customize.CLASSES.OPTION_DISABLED, !theme.customBackgroundConfigured);
  $(customize.IDS.RESTORE_DEFAULT).tabIndex =
      (theme.customBackgroundConfigured ? 0 : -1);

  $(customize.IDS.EDIT_BG)
      .classList.toggle(
          CLASSES.ENTRY_POINT_ENHANCED, !theme.customBackgroundConfigured);

  if (configData.isGooglePage) {
    customize.onThemeChange();
  }

  if (configData.realboxMatchOmniboxTheme) {
    // TODO(dbeam): actually get these from theme service.
    const removeMatchHovered = assert(theme.searchBox.icon).slice();
    removeMatchHovered[3] = .16 * 255;

    const removeMatchSelectedHovered =
        assert(theme.searchBox.iconSelected).slice();
    removeMatchSelectedHovered[3] = .16 * 255;

    const removeMatchFocused = theme.searchBox.iconSelected.slice();
    removeMatchFocused[3] = .32 * 255;

    /**
     * @param {string} varName
     * @param {!Array<number>|undefined} colors
     */
    const setCssVar = (varName, colors) => {
      const rgba = convertToRGBAColor(assert(colors));
      document.body.style.setProperty(`--${varName}`, rgba);
    };

    setCssVar('search-box-bg', theme.searchBox.bg);
    setCssVar('search-box-icon', theme.searchBox.icon);
    setCssVar('search-box-icon-selected', theme.searchBox.iconSelected);
    setCssVar('search-box-placeholder', theme.searchBox.placeholder);
    setCssVar('search-box-results-bg', theme.searchBox.resultsBg);
    setCssVar(
        'search-box-results-bg-hovered', theme.searchBox.resultsBgHovered);
    setCssVar(
        'search-box-results-bg-selected', theme.searchBox.resultsBgSelected);
    setCssVar('search-box-results-dim', theme.searchBox.resultsDim);
    setCssVar(
        'search-box-results-dim-selected', theme.searchBox.resultsDimSelected);
    setCssVar('search-box-results-text', theme.searchBox.resultsText);
    setCssVar(
        'search-box-results-text-selected',
        theme.searchBox.resultsTextSelected);
    setCssVar('search-box-results-url', theme.searchBox.resultsUrl);
    setCssVar(
        'search-box-results-url-selected', theme.searchBox.resultsUrlSelected);
    setCssVar('search-box-text', theme.searchBox.text);
    setCssVar('remove-match-hovered', removeMatchHovered);
    setCssVar('remove-match-selected-hovered', removeMatchSelectedHovered);
    setCssVar('remove-match-focused', removeMatchFocused);
  }
}

/**
 * Request data for search suggestions, promo, and the OGB. Insert it into
 * the page once it's available. For search suggestions this should be almost
 * immediately as cached data is always used. Promos and the OGB may need
 * to wait for the asynchronous network request to complete.
 */
function requestAndInsertGoogleResources() {
  if (!$('search-suggestions-loader')) {
    const ssScript = document.createElement('script');
    ssScript.id = 'search-suggestions-loader';
    ssScript.src = 'chrome-search://local-ntp/search-suggestions.js';
    ssScript.async = false;
    document.body.appendChild(ssScript);
    ssScript.onload = function() {
      injectSearchSuggestions(searchSuggestions);
    };
  }
  if (!$('one-google-loader')) {
    // Load the OneGoogleBar script. It'll create a global variable |og| which
    // is a JSON object corresponding to the native OneGoogleBarData type.
    const ogScript = document.createElement('script');
    ogScript.id = 'one-google-loader';
    ogScript.src = 'chrome-search://local-ntp/one-google.js';
    document.body.appendChild(ogScript);
    ogScript.onload = function() {
      injectOneGoogleBar(og);
    };
  }
  if (!$('promo-loader')) {
    const promoScript = document.createElement('script');
    promoScript.id = 'promo-loader';
    promoScript.src = 'chrome-search://local-ntp/promo.js';
    document.body.appendChild(promoScript);
    promoScript.onload = function() {
      injectPromo(promo);
    };
  }
}

/**
 * @param {!EventTarget} matchElToSelect
 * @return {number} The selected index (if found); else -1.
 */
function selectMatchEl(matchElToSelect) {
  let selectedIndex = -1;
  Array.from(matchEls).forEach((matchEl, i) => {
    const found = matchEl === matchElToSelect;
    matchEl.classList.toggle(CLASSES.SELECTED, found);
    matchEl.setAttribute('aria-selected', found);
    if (found) {
      selectedIndex = i;
    }
  });

  const matches = autocompleteResult ? autocompleteResult.matches : [];
  setRealboxIcon(matches[selectedIndex]);

  return selectedIndex;
}

/** Sends the current theme info to the most visited iframe. */
function sendNtpThemeToMostVisitedIframe() {
  const info = getNtpTheme();
  if (!info) {
    return;
  }

  const message = {cmd: 'updateTheme'};
  message.isThemeDark = info.isNtpBackgroundDark;
  message.customBackground = info.customBackgroundConfigured;
  message.useTitleContainer = info.useTitleContainer;
  message.iconBackgroundColor = convertToRGBAColor(info.iconBackgroundColor);
  message.useWhiteAddIcon = info.useWhiteAddIcon;

  let titleColor = NTP_DESIGN.titleColor;
  if (!info.usingDefaultTheme && info.textColorRgba) {
    titleColor = info.textColorRgba;
  } else if (info.isNtpBackgroundDark) {
    titleColor = NTP_DESIGN.titleColorAgainstDark;
  }
  message.tileTitleColor = convertToRGBAColor(titleColor);

  const iframe = $(IDS.TILES_IFRAME);
  if (iframe) {
    iframe.contentWindow.postMessage(message, '*');
  }
}

/**
 * Sets the visibility of the theme attribution.
 * @param {boolean} show True to show the attribution.
 */
function setAttributionVisibility(show) {
  $(IDS.ATTRIBUTION).style.display = show ? '' : 'none';
}

/**
 * Updates the NTP style according to theme.
 * @param {Object} ntpTheme The information about the theme.
 */
function setCustomThemeStyle(ntpTheme) {
  let textColor = '';
  let textColorLight = '';
  let mvxFilter = '';
  if (!ntpTheme.usingDefaultTheme) {
    textColor = convertToRGBAColor(ntpTheme.textColorRgba);
    textColorLight = convertToRGBAColor(ntpTheme.textColorLightRgba);
    mvxFilter = 'drop-shadow(0 0 0 ' + textColor + ')';
  }

  document.body.style.setProperty('--text-color', textColor);
  document.body.style.setProperty('--text-color-light', textColorLight);
}

/**
 * @param {boolean} focus True to show a dragging focus on the fakebox.
 */
function setFakeboxDragFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_DRAG_FOCUS, focus);
}

/**
 * @param {boolean} focus True to focus the fakebox.
 */
function setFakeboxFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_FOCUS, focus);
}

/**
 * @param {boolean} show True to show the fakebox and logo.
 */
function setFakeboxVisibility(show) {
  document.body.classList.toggle(CLASSES.HIDE_FAKEBOX, !show);
}

/**
 * @param {!Element} element
 * @param {string} url
 */
function setBackgroundImageByUrl(element, url) {
  element.style.webkitMaskImage = '';
  element.style.backgroundImage = `url(${url})`;
  element.style.backgroundColor = 'transparent';
}

/**
 * @param {!Element} element
 * @param {string} url
 */
function setWebkitMaskImageByUrl(element, url) {
  element.style.webkitMaskImage = `url(${url})`;
  element.style.backgroundImage = '';
  element.style.backgroundColor = '';
}

/** @param {!AutocompleteMatch|undefined} match */
function setRealboxIcon(match) {
  const realboxIcon = $(IDS.REALBOX_ICON);
  if (match && !match.isSearchType) {
    // if the selected match is a navigation match and has a favicon loaded,
    // display the favicon. Otherwise display the match icon.
    if (faviconOrImageUrlToDataUrlCache[match.destinationUrl]) {
      realboxIcon.dataset.icon = '';
      setBackgroundImageByUrl(
          realboxIcon, faviconOrImageUrlToDataUrlCache[match.destinationUrl]);
    } else if (match.type == DOCUMENT_MATCH_TYPE) {
      realboxIcon.dataset.icon = match.iconUrl;
      // Document matches use colored SVG icons.
      setBackgroundImageByUrl(realboxIcon, realboxIcon.dataset.icon);
    } else {
      realboxIcon.dataset.icon = match.iconUrl;
      setWebkitMaskImageByUrl(realboxIcon, realboxIcon.dataset.icon);
    }
  } else if (configData.useGoogleGIcon) {
    // if google_g icon should be used (as the default icon and search matches),
    // display the default icon which is set to the google_g icon.
    /** @suppress {missingProperties} */
    realboxIcon.dataset.icon = realboxIcon.dataset.defaultIcon;
    setBackgroundImageByUrl(realboxIcon, realboxIcon.dataset.icon);
  } else {
    // if no match is selected, display the default icon. Otherwise display the
    // match icon.
    /** @suppress {missingProperties} */
    realboxIcon.dataset.icon =
        match ? match.iconUrl : realboxIcon.dataset.defaultIcon;
    setWebkitMaskImageByUrl(realboxIcon, realboxIcon.dataset.icon);
  }
}

/** @param {boolean} visible */
function setRealboxMatchesVisible(visible) {
  $(IDS.REALBOX_INPUT_WRAPPER).classList.toggle(CLASSES.SHOW_MATCHES, visible);
}

/**
 * Shows the error pop-up notification and triggers a delay to hide it. The
 * message will be set to |msg|. If |linkName| and |linkOnClick| are present,
 * shows an error link with text set to |linkName| and onclick handled by
 * |linkOnClick|.
 * @param {string} msg The notification message.
 * @param {?string} linkName The error link text.
 * @param {?Function} linkOnClick The error link onclick handler.
 */
function showErrorNotification(msg, linkName, linkOnClick) {
  const notification = $(IDS.ERROR_NOTIFICATION);
  $(IDS.ERROR_NOTIFICATION_MSG).textContent = msg;
  if (linkName && linkOnClick) {
    const notificationLink = $(IDS.ERROR_NOTIFICATION_LINK);
    notificationLink.textContent = linkName;
    notificationLink.onclick = linkOnClick;
    notification.classList.add(CLASSES.HAS_LINK);
  } else {
    notification.classList.remove(CLASSES.HAS_LINK);
  }
  floatUpNotification(notification, $(IDS.ERROR_NOTIFICATION_CONTAINER));
}

/**
 * Shows the Most Visited pop-up notification and triggers a delay to hide it.
 * The message will be set to |msg|.
 * @param {string} msg The notification message.
 */
function showNotification(msg) {
  $(IDS.NOTIFICATION_MESSAGE).textContent = msg;
  $(IDS.RESTORE_ALL_LINK).textContent = customLinksEnabled() ?
      configData.translatedStrings.restoreDefaultLinks :
      configData.translatedStrings.restoreThumbnailsShort;
  floatUpNotification($(IDS.NOTIFICATION), $(IDS.NOTIFICATION_CONTAINER));
  $(IDS.UNDO_LINK).focus();
}

function showPromoIfNotOverlapping() {
  $(IDS.PROMO).style.visibility = isPromoOverlapping() ? 'hidden' : 'visible';
}

function showPromoIfNotOverlappingAndTrackResizes() {
  showPromoIfNotOverlapping();
  // The removal before addition is to ensure only 1 event listener is ever
  // active at the same time.
  window.removeEventListener('resize', showPromoIfNotOverlapping);
  window.addEventListener('resize', showPromoIfNotOverlapping);
}

/**
 * @param {string} text
 * @param {!Array<string>} classes
 * @return {!Element}
 */
function spanWithClasses(text, classes) {
  const span = document.createElement('span');
  span.classList.add(...classes);
  span.textContent = text;
  return span;
}

/** @param {!RealboxOutputUpdate} update */
function updateRealboxOutput(update) {
  assert(Object.keys(update).length > 0);

  const realboxEl = $(IDS.REALBOX);
  const newOutput =
      /** @type {!RealboxOutput} */ (Object.assign({}, lastOutput, update));
  const newAll = newOutput.text + newOutput.inline;

  const inlineDiffers = newOutput.inline !== lastOutput.inline;
  const preserveSelection = !inlineDiffers && !update.moveCursorToEnd;
  let needsSelectionUpdate = !preserveSelection;

  const oldSelectionStart = realboxEl.selectionStart;

  if (newAll !== realboxEl.value) {
    realboxEl.value = newAll;
    needsSelectionUpdate = true;  // Setting .value blows away selection.
  }

  if (newAll.trim() && needsSelectionUpdate) {
    realboxEl.selectionStart =
        preserveSelection ? oldSelectionStart : newOutput.text.length;
    // If the selection shouldn't be preserved, set the selection end to the
    // same as the selection start (i.e. drop selection but move cursor).
    realboxEl.selectionEnd =
        preserveSelection ? oldSelectionStart : newAll.length;
  }

  isDeletingInput = userDeletedOutput(lastOutput, newOutput);
  lastOutput = newOutput;
}

/**
 * Renders the attribution if the URL is present, otherwise hides it.
 * @param {string|undefined} url The URL of the attribution image, if any.
 * @param {string|undefined} themeBackgroundAlignment The alignment of the theme
 *     background image. This is used to compute the attribution's alignment.
 */
function updateThemeAttribution(url, themeBackgroundAlignment) {
  if (!url) {
    setAttributionVisibility(false);
    return;
  }

  const attribution = $(IDS.ATTRIBUTION);
  let attributionImage = attribution.querySelector('img');
  if (!attributionImage) {
    attributionImage = new Image();
    attribution.appendChild(attributionImage);
  }
  attributionImage.style.content = url;

  // To avoid conflicts, place the attribution on the left for themes that
  // right align their background images.
  attribution.classList.toggle(
      CLASSES.LEFT_ALIGN_ATTRIBUTION, themeBackgroundAlignment == 'right');
  setAttributionVisibility(true);
}

/**
 * @param {!RealboxOutput} before
 * @param {!RealboxOutput} after
 * @return {boolean}
 */
function userDeletedOutput(before, after) {
  const beforeAll = before.text + before.inline;
  const afterAll = after.text + after.inline;
  return beforeAll.length > afterAll.length && beforeAll.startsWith(afterAll);
}

return {
  init: init,  // Exposed for testing.
  listen: listen,
  disableIframesAndVoiceSearchForTesting:
      disableIframesAndVoiceSearchForTesting,
  overrideExecutableTimeoutForTesting: overrideExecutableTimeoutForTesting
};
}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
