// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview The local InstantExtended NTP.
 */


/**
 * Whether the most visited tiles have finished loading, i.e. we've received the
 * 'loaded' postMessage from the iframe. Used by tests to detect that loading
 * has completed.
 * @type {boolean}
 */
var tilesAreLoaded = false;


var numDdllogResponsesReceived = 0;
var lastDdllogResponse = '';

var onDdllogResponse = null;


/**
 * Whether the Most Visited and edit custom link iframes should be created while
 * running tests. Currently the SimpleJavascriptTests are flaky due to some
 * raciness in the creation/destruction of the iframe. crbug.com/786313.
 * @type {boolean}
 */
let iframesAndVoiceSearchDisabledForTesting = false;


/**
 * Controls rendering the new tab page for InstantExtended.
 * @return {Object} A limited interface for testing the local NTP.
 */
function LocalNTP() {
'use strict';


/**
 * Called by tests to disable the creation of Most Visited and edit custom link
 * iframes.
 */
function disableIframesAndVoiceSearchForTesting() {
  iframesAndVoiceSearchDisabledForTesting = true;
}


/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  // eslint-disable-next-line no-restricted-properties
  return document.getElementById(id);
}


/**
 * Specifications for an NTP design (not comprehensive).
 *
 * numTitleLines: Number of lines to display in titles.
 * titleColor: The 4-component color of title text.
 * titleColorAgainstDark: The 4-component color of title text against a dark
 *   theme.
 *
 * @type {{
 *   numTitleLines: number,
 *   titleColor: string,
 *   titleColorAgainstDark: string,
 * }}
 */
var NTP_DESIGN = {
  numTitleLines: 1,
  titleColor: [50, 50, 50, 255],
  titleColorAgainstDark: [210, 210, 210, 255],
};


/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
var CLASSES = {
  ALTERNATE_LOGO: 'alternate-logo',  // Shows white logo if required by theme
  DARK: 'dark',
  DEFAULT_THEME: 'default-theme',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  FADE: 'fade',  // Enables opacity transition on logo and doodle.
  FAKEBOX_FOCUS: 'fakebox-focused',  // Applies focus styles to the fakebox
  SHOW_EDIT_DIALOG: 'show',          // Displays the edit custom link dialog.
  HIDE_BODY_OVERFLOW: 'hidden',      // Prevents scrolling while the edit custom
                                     // link dialog is open.
  // Applies float animations to the Most Visited notification
  FLOAT_UP: 'float-up',
  // Applies ripple animation to the element on click
  RIPPLE: 'ripple',
  RIPPLE_CONTAINER: 'ripple-container',
  RIPPLE_EFFECT_MASK: 'ripple-effect-mask',
  RIPPLE_EFFECT: 'ripple-effect',
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  // Applies a different style to the error notification if a link is present.
  HAS_LINK: 'has-link',
  HIDE_FAKEBOX: 'hide-fakebox',
  HIDE_NOTIFICATION: 'notice-hide',
  INITED: 'inited',  // Reveals the <body> once init() is done.
  LEFT_ALIGN_ATTRIBUTION: 'left-align-attribution',
  MATERIAL_DESIGN: 'md',  // Applies Material Design styles to the page
  MATERIAL_DESIGN_ICONS:
      'md-icons',  // Applies Material Design styles to Most Visited.
  // Vertically centers the most visited section for a non-Google provided page.
  NON_GOOGLE_PAGE: 'non-google-page',
  NON_WHITE_BG: 'non-white-bg',
  RTL: 'rtl',              // Right-to-left language text.
  SHOW_LOGO: 'show-logo',  // Marks logo/doodle that should be shown.
};


/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
var IDS = {
  ATTRIBUTION: 'attribution',
  ATTRIBUTION_TEXT: 'attribution-text',
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
  LOGO: 'logo',
  LOGO_DEFAULT: 'logo-default',
  LOGO_DOODLE: 'logo-doodle',
  LOGO_DOODLE_IMAGE: 'logo-doodle-image',
  LOGO_DOODLE_IFRAME: 'logo-doodle-iframe',
  LOGO_DOODLE_BUTTON: 'logo-doodle-button',
  LOGO_DOODLE_NOTIFIER: 'logo-doodle-notifier',
  MOST_VISITED: 'most-visited',
  NOTIFICATION: 'mv-notice',
  NOTIFICATION_CONTAINER: 'mv-notice-container',
  NOTIFICATION_CLOSE_BUTTON: 'mv-notice-x',
  NOTIFICATION_MESSAGE: 'mv-msg',
  NTP_CONTENTS: 'ntp-contents',
  RESTORE_ALL_LINK: 'mv-restore',
  TILES: 'mv-tiles',
  TILES_IFRAME: 'mv-single',
  UNDO_LINK: 'mv-undo',
};


/**
 * Counterpart of search_provider_logos::LogoType.
 * @enum {string}
 * @const
 */
var LOGO_TYPE = {
  SIMPLE: 'SIMPLE',
  ANIMATED: 'ANIMATED',
  INTERACTIVE: 'INTERACTIVE',
};


/**
 * The different types of events that are logged from the NTP. This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
var LOG_TYPE = {
  // A static Doodle was shown, coming from cache.
  NTP_STATIC_LOGO_SHOWN_FROM_CACHE: 30,
  // A static Doodle was shown, coming from the network.
  NTP_STATIC_LOGO_SHOWN_FRESH: 31,
  // A call-to-action Doodle image was shown, coming from cache.
  NTP_CTA_LOGO_SHOWN_FROM_CACHE: 32,
  // A call-to-action Doodle image was shown, coming from the network.
  NTP_CTA_LOGO_SHOWN_FRESH: 33,

  // A static Doodle was clicked.
  NTP_STATIC_LOGO_CLICKED: 34,
  // A call-to-action Doodle was clicked.
  NTP_CTA_LOGO_CLICKED: 35,
  // An animated Doodle was clicked.
  NTP_ANIMATED_LOGO_CLICKED: 36,

  // The One Google Bar was shown.
  NTP_ONE_GOOGLE_BAR_SHOWN: 37,

  // 'Cancel' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_CANCEL: 54,
  // 'Done' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_DONE: 55,
};


/**
 * The maximum number of tiles to show in the Most Visited section.
 * @type {number}
 * @const
 */
const MAX_NUM_TILES_MOST_VISITED = 8;


/**
 * The maximum number of tiles to show in the Most Visited section if custom
 * links is enabled.
 * @type {number}
 * @const
 */
const MAX_NUM_TILES_CUSTOM_LINKS = 10;


/**
 * Background colors considered "white". Used to determine if it is possible
 * to display a Google Doodle, or if the notifier should be used instead.
 * @type {Array<string>}
 * @const
 */
var WHITE_BACKGROUND_COLORS = ['rgba(255,255,255,1)', 'rgba(0,0,0,0)'];

/**
 * Enum for keycodes.
 * @enum {number}
 * @const
 */
var KEYCODE = {ENTER: 13, SPACE: 32};


/**
 * The period of time (ms) before the Most Visited notification is hidden.
 * @type {number}
 */
const NOTIFICATION_TIMEOUT = 10000;


/**
 * The duration of the ripple animation.
 * @type {number}
 */
const RIPPLE_DURATION_MS = 800;


/**
 * The max size of the ripple animation.
 * @type {number}
 */
const RIPPLE_MAX_RADIUS_PX = 300;


/**
 * The last blacklisted tile rid if any, which by definition should not be
 * filler.
 * @type {?number}
 */
var lastBlacklistedTile = null;


/**
 * The timeout function for automatically hiding the pop-up notification. Only
 * set if a notification is visible.
 * @type {?Object}
 */
let delayedHideNotification;


/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
var ntpApiHandle;


/**
 * Returns a timeout that can be executed early.
 * @param {!Function} timeout The timeout function.
 * @param {number} delay The timeout delay.
 * @param {Object} previousContainer The pre-existing notification container.
 * @return {Object}
 */
function createExecutableTimeout(timeout, delay, previousContainer) {
  let timeoutId = window.setTimeout(timeout, delay);
  return {
    previousContainer: previousContainer,
    clear: () => {
      window.clearTimeout(timeoutId);
    },
    trigger: () => {
      window.clearTimeout(timeoutId);
      return timeout();
    }
  };
}


/**
 * Returns theme background info, first checking for history.state.notheme. If
 * the page has notheme set, returns a fallback light-colored theme.
 */
function getThemeBackgroundInfo() {
  if (history.state && history.state.notheme) {
    return {
      alternateLogo: false,
      backgroundColorRgba: [255, 255, 255, 255],
      colorRgba: [255, 255, 255, 255],
      headerColorRgba: [150, 150, 150, 255],
      linkColorRgba: [6, 55, 116, 255],
      sectionBorderColorRgba: [150, 150, 150, 255],
      textColorLightRgba: [102, 102, 102, 255],
      textColorRgba: [0, 0, 0, 255],
      usingDefaultTheme: true,
    };
  }
  return ntpApiHandle.themeBackgroundInfo;
}


/**
 * Heuristic to determine whether a theme should be considered to be dark, so
 * the colors of various UI elements can be adjusted.
 * @param {ThemeBackgroundInfo|undefined} info Theme background information.
 * @return {boolean} Whether the theme is dark.
 * @private
 */
function getIsThemeDark() {
  var info = getThemeBackgroundInfo();
  if (!info)
    return false;
  // Heuristic: light text implies dark theme.
  var rgba = info.textColorRgba;
  var luminance = 0.3 * rgba[0] + 0.59 * rgba[1] + 0.11 * rgba[2];
  return luminance >= 128;
}

/**
 * Updates the NTP based on the current theme.
 * @private
 */
function renderTheme() {
  $(IDS.NTP_CONTENTS).classList.toggle(CLASSES.DARK, getIsThemeDark());

  var info = getThemeBackgroundInfo();
  if (!info)
    return;

  var background = [convertToRGBAColor(info.backgroundColorRgba),
                    info.imageUrl,
                    info.imageTiling,
                    info.imageHorizontalAlignment,
                    info.imageVerticalAlignment].join(' ').trim();

  // If a custom background has been selected the image will be applied to the
  // custom-background element instead of the body.
  if (!info.customBackgroundConfigured) {
    document.body.style.background = background;
  }
  document.body.classList.toggle(CLASSES.ALTERNATE_LOGO, info.alternateLogo);
  var isNonWhiteBackground = !WHITE_BACKGROUND_COLORS.includes(background);
  document.body.classList.toggle(CLASSES.NON_WHITE_BG, isNonWhiteBackground);
  updateThemeAttribution(info.attributionUrl, info.imageHorizontalAlignment);
  setCustomThemeStyle(info);

  if (info.customBackgroundConfigured) {
    var imageWithOverlay = [
      customBackgrounds.CUSTOM_BACKGROUND_OVERLAY, 'url(' + info.imageUrl + ')'
    ].join(',').trim();

    if (imageWithOverlay != document.body.style.backgroundImage) {
      customBackgrounds.closeCustomizationDialog();
      customBackgrounds.clearAttribution();
    }

    // |image| and |imageWithOverlay| use the same url as their source. Waiting
    // to display the custom background until |image| is fully loaded ensures
    // that |imageWithOverlay| is also loaded.
    $('custom-bg').style.backgroundImage = imageWithOverlay;
    var image = new Image();
    image.onload = function() {
      $('custom-bg').style.opacity = '1';
    };
    image.src = info.imageUrl;

    customBackgrounds.setAttribution(
        info.attribution1, info.attribution2, info.attributionActionUrl);
  } else {
    $('custom-bg').style.opacity = '0';
    window.setTimeout(function() {
      $('custom-bg').style.backgroundImage = '';
    }, 1000);
    customBackgrounds.clearAttribution();
  }

  $(customBackgrounds.IDS.RESTORE_DEFAULT)
      .classList.toggle(
          customBackgrounds.CLASSES.OPTION_DISABLED,
          !info.customBackgroundConfigured);
  $(customBackgrounds.IDS.RESTORE_DEFAULT).tabIndex =
      (info.customBackgroundConfigured ? 0 : -1);

  if (configData.isGooglePage) {
    // Hide the settings menu or individual options if the related features are
    // disabled.
    customBackgrounds.setMenuVisibility();
  }
}

/**
 * Sends the current theme info to the most visited iframe.
 * @private
 */
function sendThemeInfoToMostVisitedIframe() {
  var info = getThemeBackgroundInfo();
  if (!info)
    return;

  var isThemeDark = getIsThemeDark();

  var message = {cmd: 'updateTheme'};
  message.isThemeDark = isThemeDark;
  message.isUsingTheme = !info.usingDefaultTheme;

  var titleColor = NTP_DESIGN.titleColor;
  if (!info.usingDefaultTheme && info.textColorRgba) {
    titleColor = info.textColorRgba;
  } else if (isThemeDark) {
    titleColor = NTP_DESIGN.titleColorAgainstDark;
  }
  message.tileTitleColor = convertToRGBAColor(titleColor);

  $(IDS.TILES_IFRAME).contentWindow.postMessage(message, '*');
}


/**
 * Updates the OneGoogleBar (if it is loaded) based on the current theme.
 * @private
 */
function renderOneGoogleBarTheme() {
  if (!window.gbar) {
    return;
  }
  try {
    var oneGoogleBarApi = window.gbar.a;
    var oneGoogleBarPromise = oneGoogleBarApi.bf();
    oneGoogleBarPromise.then(function(oneGoogleBar) {
      var isThemeDark = getIsThemeDark();
      var setForegroundStyle = oneGoogleBar.pc.bind(oneGoogleBar);
      setForegroundStyle(isThemeDark ? 1 : 0);
    });
  } catch (err) {
    console.log('Failed setting OneGoogleBar theme:\n' + err);
  }
}


/**
 * Callback for embeddedSearch.newTabPage.onthemechange.
 * @private
 */
function onThemeChange() {
  renderTheme();
  renderOneGoogleBarTheme();
  sendThemeInfoToMostVisitedIframe();
}


/**
 * Updates the NTP style according to theme.
 * @param {Object} themeInfo The information about the theme.
 * @private
 */
function setCustomThemeStyle(themeInfo) {
  var textColor = null;
  var textColorLight = null;
  var mvxFilter = null;
  if (!themeInfo.usingDefaultTheme) {
    textColor = convertToRGBAColor(themeInfo.textColorRgba);
    textColorLight = convertToRGBAColor(themeInfo.textColorLightRgba);
    mvxFilter = 'drop-shadow(0 0 0 ' + textColor + ')';
  }

  $(IDS.NTP_CONTENTS)
      .classList.toggle(CLASSES.DEFAULT_THEME, themeInfo.usingDefaultTheme);

  document.body.style.setProperty('--text-color', textColor);
  document.body.style.setProperty('--text-color-light', textColorLight);
  // Themes reuse the "light" text color for links too.
  document.body.style.setProperty('--text-color-link', textColorLight);
  $(IDS.NOTIFICATION_CLOSE_BUTTON)
      .style.setProperty('--theme-filter', mvxFilter);
}


/**
 * Renders the attribution if the URL is present, otherwise hides it.
 * @param {string} url The URL of the attribution image, if any.
 * @param {string} themeBackgroundAlignment The alignment of the theme
 *  background image. This is used to compute the attribution's alignment.
 * @private
 */
function updateThemeAttribution(url, themeBackgroundAlignment) {
  if (!url) {
    setAttributionVisibility_(false);
    return;
  }

  var attribution = $(IDS.ATTRIBUTION);
  var attributionImage = attribution.querySelector('img');
  if (!attributionImage) {
    attributionImage = new Image();
    attribution.appendChild(attributionImage);
  }
  attributionImage.style.content = url;

  // To avoid conflicts, place the attribution on the left for themes that
  // right align their background images.
  attribution.classList.toggle(CLASSES.LEFT_ALIGN_ATTRIBUTION,
                               themeBackgroundAlignment == 'right');
  setAttributionVisibility_(true);
}


/**
 * Sets the visibility of the theme attribution.
 * @param {boolean} show True to show the attribution.
 * @private
 */
function setAttributionVisibility_(show) {
  $(IDS.ATTRIBUTION).style.display = show ? '' : 'none';
}


/**
 * Converts an Array of color components into RGBA format "rgba(R,G,B,A)".
 * @param {Array<number>} color Array of rgba color components.
 * @return {string} CSS color in RGBA format.
 * @private
 */
function convertToRGBAColor(color) {
  return 'rgba(' + color[0] + ',' + color[1] + ',' + color[2] + ',' +
                    color[3] / 255 + ')';
}


/**
 * Callback for embeddedSearch.newTabPage.onmostvisitedchange. Called when the
 * NTP tiles are updated.
 */
function onMostVisitedChange() {
  reloadTiles();
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

  var pages = ntpApiHandle.mostVisited;
  var cmds = [];
  let maxNumTiles = configData.isCustomLinksEnabled ?
      MAX_NUM_TILES_CUSTOM_LINKS :
      MAX_NUM_TILES_MOST_VISITED;
  for (var i = 0; i < Math.min(maxNumTiles, pages.length); ++i) {
    cmds.push({cmd: 'tile', rid: pages[i].rid});
  }
  cmds.push({cmd: 'show'});

  $(IDS.TILES_IFRAME).contentWindow.postMessage(cmds, '*');
}


/**
 * Callback for embeddedSearch.newTabPage.onaddcustomlinkdone. Called when the
 * custom link was successfully added. Shows the "Shortcut added" notification.
 * @param {boolean} success True if the link was successfully added.
 */
function onAddCustomLinkDone(success) {
  if (success)
    showNotification(configData.translatedStrings.linkAddedMsg);
  else
    showErrorNotification(configData.translatedStrings.linkCantCreate);
  ntpApiHandle.logEvent(LOG_TYPE.NTP_CUSTOMIZE_SHORTCUT_DONE);
}


/**
 * Callback for embeddedSearch.newTabPage.onupdatecustomlinkdone. Called when
 * the custom link was successfully updated. Shows the "Shortcut edited"
 * notification.
 * @param {boolean} success True if the link was successfully updated.
 */
function onUpdateCustomLinkDone(success) {
  if (success)
    showNotification(configData.translatedStrings.linkEditedMsg);
  else
    showErrorNotification(configData.translatedStrings.linkCantEdit);
}


/**
 * Callback for embeddedSearch.newTabPage.ondeletecustomlinkdone. Called when
 * the custom link was successfully deleted. Shows the "Shortcut deleted"
 * notification.
 * @param {boolean} success True if the link was successfully deleted.
 */
function onDeleteCustomLinkDone(success) {
  if (success)
    showNotification(configData.translatedStrings.linkRemovedMsg);
  else
    showErrorNotification(configData.translatedStrings.linkCantRemove);
}


/**
 * Callback for embeddedSearch.newTabPage.ondoesurlresolve. Called when we
 * determine if a custom link URL can resolve. Notifies the edit custom link
 * dialog with the result.
 * @param {boolean} resolves True if the URL can resolve.
 */
function onDoesUrlResolve(resolves) {
  $(IDS.CUSTOM_LINKS_EDIT_IFRAME)
      .contentWindow.postMessage(
          {cmd: 'doesUrlResolve', resolves: resolves}, '*');
}


/**
 * Shows the Most Visited pop-up notification and triggers a delay to hide it.
 * The message will be set to |msg|.
 * @param {string} msg The notification message.
 */
function showNotification(msg) {
  $(IDS.NOTIFICATION_MESSAGE).textContent = msg;

  if (configData.isMDIconsEnabled && configData.isGooglePage) {
    floatUpNotification($(IDS.NOTIFICATION), $(IDS.NOTIFICATION_CONTAINER));
  } else {
    var notification = $(IDS.NOTIFICATION);
    notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
    notification.classList.remove(CLASSES.DELAYED_HIDE_NOTIFICATION);
    notification.scrollTop;
    notification.classList.add(CLASSES.DELAYED_HIDE_NOTIFICATION);
  }

  $(IDS.UNDO_LINK).focus();
}


/**
 * Hides the Most Visited pop-up notification.
 */
function hideNotification() {
  if (configData.isMDIconsEnabled && configData.isGooglePage) {
    floatDownNotification($(IDS.NOTIFICATION), $(IDS.NOTIFICATION_CONTAINER));
  } else {
    var notification = $(IDS.NOTIFICATION);
    notification.classList.add(CLASSES.HIDE_NOTIFICATION);
    notification.classList.remove(CLASSES.DELAYED_HIDE_NOTIFICATION);
  }
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
  let notification = $(IDS.ERROR_NOTIFICATION);
  $(IDS.ERROR_NOTIFICATION_MSG).textContent = msg;
  if (linkName && linkOnClick) {
    let notificationLink = $(IDS.ERROR_NOTIFICATION_LINK);
    notificationLink.textContent = linkName;
    notificationLink.onclick = linkOnClick;
    notification.classList.add(CLASSES.HAS_LINK);
  } else {
    notification.classList.remove(CLASSES.HAS_LINK);
  }
  floatUpNotification(notification, $(IDS.ERROR_NOTIFICATION_CONTAINER));
}


/**
 * Animates the specified notification to float up. Automatically hides any
 * pre-existing notification and sets a delayed timer to hide the new
 * notification.
 * @param {!Element} notification The notification element.
 * @param {!Element} notificationContainer The notification container element.
 */
function floatUpNotification(notification, notificationContainer) {
  // Hide pre-existing notification if it was different type. Clear timeout and
  // replace it with the new timeout and new message if it was the same type.
  if (delayedHideNotification) {
    if (delayedHideNotification.previousContainer === notificationContainer)
      delayedHideNotification.clear();
    else
      delayedHideNotification.trigger();
    delayedHideNotification = null;
  }

  notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
  // Timeout is required for the "float up" transition to work. Modifying the
  // "display" property prevents transitions from activating.
  window.setTimeout(() => {
    notificationContainer.classList.add(CLASSES.FLOAT_UP);
  }, 20);

  // Automatically hide the notification after a period of time.
  delayedHideNotification = createExecutableTimeout(() => {
    floatDownNotification(notification, notificationContainer);
  }, NOTIFICATION_TIMEOUT, notificationContainer);
}


/**
 * Animates the pop-up notification to float down, and clears the timeout to
 * hide the notification.
 * @param {!Element} notification The notification element.
 * @param {!Element} notificationContainer The notification container element.
 */
function floatDownNotification(notification, notificationContainer) {
  if (!notificationContainer.classList.contains(CLASSES.FLOAT_UP))
    return;

  // Clear the timeout to hide the notification.
  if (delayedHideNotification) {
    delayedHideNotification.clear();
    delayedHideNotification = null;
  }

  // Reset notification visibility once the animation is complete.
  notificationContainer.classList.remove(CLASSES.FLOAT_UP);
  let afterHide = (event) => {
    if (event.propertyName === 'bottom') {
      notification.classList.add(CLASSES.HIDE_NOTIFICATION);
      notification.classList.remove(CLASSES.HAS_LINK);
      notificationContainer.removeEventListener('transitionend', afterHide);
    }
    // Blur the hidden items.
    $(IDS.UNDO_LINK).blur();
    $(IDS.RESTORE_ALL_LINK).blur();
  };
  notificationContainer.addEventListener('transitionend', afterHide);
}


/**
 * Handles a click on the notification undo link by hiding the notification and
 * informing Chrome.
 */
function onUndo() {
  hideNotification();
  // Focus on the omnibox after the notification is hidden.
  window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes();
  if (configData.isCustomLinksEnabled) {
    ntpApiHandle.undoCustomLinkAction();
  } else if (lastBlacklistedTile != null) {
    ntpApiHandle.undoMostVisitedDeletion(lastBlacklistedTile);
  }
}


/**
 * Handles a click on the restore all notification link by hiding the
 * notification and informing Chrome.
 */
function onRestoreAll() {
  hideNotification();
  // Focus on the omnibox after the notification is hidden.
  window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes();
  if (configData.isCustomLinksEnabled) {
    ntpApiHandle.resetCustomLinks();
  } else {
    ntpApiHandle.undoAllMostVisitedDeletions();
  }
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
 * Callback for embeddedSearch.newTabPage.oninputcancel. Restores the NTP
 * (re-enables the fakebox and unhides the logo.)
 */
function onInputCancel() {
  setFakeboxVisibility(true);
}


/**
 * @param {boolean} focus True to focus the fakebox.
 */
function setFakeboxFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_FOCUS, focus);
}

/**
 * @param {boolean} focus True to show a dragging focus on the fakebox.
 */
function setFakeboxDragFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_DRAG_FOCUS, focus);
}

/**
 * @return {boolean} True if the fakebox has focus.
 */
function isFakeboxFocused() {
  return document.body.classList.contains(CLASSES.FAKEBOX_FOCUS) ||
      document.body.classList.contains(CLASSES.FAKEBOX_DRAG_FOCUS);
}


/**
 * @param {!Event} event The click event.
 * @return {boolean} True if the click occurred in an enabled fakebox.
 */
function isFakeboxClick(event) {
  return $(IDS.FAKEBOX).contains(event.target) &&
      !$(IDS.FAKEBOX_MICROPHONE).contains(event.target);
}


/**
 * @param {boolean} show True to show the fakebox and logo.
 */
function setFakeboxVisibility(show) {
  document.body.classList.toggle(CLASSES.HIDE_FAKEBOX, !show);
}


/**
 * @param {!Element} element The element to register the handler for.
 * @param {number} keycode The keycode of the key to register.
 * @param {!Function} handler The key handler to register.
 */
function registerKeyHandler(element, keycode, handler) {
  element.addEventListener('keydown', function(event) {
    if (event.keyCode == keycode)
      handler(event);
  });
}


/**
 * Event handler for messages from the most visited and edit custom link iframe.
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  var cmd = event.data.cmd;
  var args = event.data;
  if (cmd === 'loaded') {
    tilesAreLoaded = true;
    if (configData.isGooglePage && !$('one-google-loader')) {
      // Load the OneGoogleBar script. It'll create a global variable name "og"
      // which is a dict corresponding to the native OneGoogleBarData type.
      // We do this only after all the tiles have loaded, to avoid slowing down
      // the main page load.
      var ogScript = document.createElement('script');
      ogScript.id = 'one-google-loader';
      ogScript.src = 'chrome-search://local-ntp/one-google.js';
      document.body.appendChild(ogScript);
      ogScript.onload = function() {
        injectOneGoogleBar(og);
      };
    }
    if (configData.isCustomLinksEnabled) {
      $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT)
          .classList.toggle(
              customBackgrounds.CLASSES.OPTION_DISABLED,
              !args.showRestoreDefault);
      $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).tabIndex =
          (args.showRestoreDefault ? 0 : -1);
    }
  } else if (cmd === 'tileBlacklisted') {
    if (configData.isCustomLinksEnabled) {
      showNotification(configData.translatedStrings.linkRemovedMsg);
    } else {
      showNotification(
          configData.translatedStrings.thumbnailRemovedNotification);
    }
    lastBlacklistedTile = args.tid;

    ntpApiHandle.deleteMostVisitedItem(args.tid);
  } else if (cmd === 'resizeDoodle') {
    let width = args.width || null;
    let height = args.height || null;
    let duration = args.duration || '0s';
    let iframe = $(IDS.LOGO_DOODLE_IFRAME);
    document.body.style.setProperty('--logo-iframe-height', height);
    document.body.style.setProperty('--logo-iframe-width', width);
    document.body.style.setProperty('--logo-iframe-resize-duration', duration);
  } else if (cmd === 'startEditLink') {
    $(IDS.CUSTOM_LINKS_EDIT_IFRAME)
        .contentWindow.postMessage({cmd: 'linkData', tid: args.tid}, '*');
    // Small delay to allow the dialog to finish setting up before displaying.
    window.setTimeout(function() {
      $(IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG).showModal();
    }, 10);
  } else if (cmd === 'closeDialog') {
    $(IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG).close();
  } else if (cmd === 'focusMenu') {
    // Focus the edited tile's menu or the add shortcut tile after closing the
    // custom link edit dialog without saving.
    $(IDS.TILES_IFRAME)
        .contentWindow.postMessage({cmd: 'focusMenu', tid: args.tid}, '*');
  }
}


/**
 * Enables Material Design styles for the Most Visited section. Implicitly
 * enables Material Design for the rest of NTP.
 */
function enableMDIcons() {
  $(IDS.MOST_VISITED).classList.add(CLASSES.MATERIAL_DESIGN_ICONS);
  $(IDS.TILES).classList.add(CLASSES.MATERIAL_DESIGN_ICONS);
  enableMD();
  addRippleAnimations();
}


/**
 * Enables Material Design styles for all NTP components except Most Visited.
 */
function enableMD() {
  document.body.classList.add(CLASSES.MATERIAL_DESIGN);
}


/**
 * Enables ripple animations for elements with CLASSES.RIPPLE. The target
 * element must have position relative or absolute.
 * TODO(kristipark): Remove after migrating to WebUI.
 */
function addRippleAnimations() {
  let ripple = (event) => {
    let target = event.target;
    const rect = target.getBoundingClientRect();
    const x = Math.round(event.clientX - rect.left);
    const y = Math.round(event.clientY - rect.top);

    // Calculate radius
    const corners = [
      {x: 0, y: 0},
      {x: rect.width, y: 0},
      {x: 0, y: rect.height},
      {x: rect.width, y: rect.height},
    ];
    let distance = (x1, y1, x2, y2) => {
      var xDelta = x1 - x2;
      var yDelta = y1 - y2;
      return Math.sqrt(xDelta * xDelta + yDelta * yDelta);
    };
    let cornerDistances = corners.map(function(corner) {
      return Math.round(distance(x, y, corner.x, corner.y));
    });
    const radius =
        Math.min(RIPPLE_MAX_RADIUS_PX, Math.max.apply(Math, cornerDistances));

    let ripple = document.createElement('div');
    let rippleMask = document.createElement('div');
    let rippleContainer = document.createElement('div');
    ripple.classList.add(CLASSES.RIPPLE_EFFECT);
    rippleMask.classList.add(CLASSES.RIPPLE_EFFECT_MASK);
    rippleContainer.classList.add(CLASSES.RIPPLE_CONTAINER);
    rippleMask.appendChild(ripple);
    rippleContainer.appendChild(rippleMask);
    target.appendChild(rippleContainer);
    // Ripple start location
    ripple.style.marginLeft = x + 'px';
    ripple.style.marginTop = y + 'px';

    rippleMask.style.width = target.offsetWidth + 'px';
    rippleMask.style.height = target.offsetHeight + 'px';
    rippleMask.style.borderRadius =
        window.getComputedStyle(target).borderRadius;

    // Start transition/ripple
    ripple.style.width = radius * 2 + 'px';
    ripple.style.height = radius * 2 + 'px';
    ripple.style.marginLeft = x - radius + 'px';
    ripple.style.marginTop = y - radius + 'px';
    ripple.style.backgroundColor = 'rgba(0, 0, 0, 0)';

    window.setTimeout(function() {
      ripple.remove();
      rippleMask.remove();
      rippleContainer.remove();
    }, RIPPLE_DURATION_MS);
  };

  let rippleElements = document.querySelectorAll('.' + CLASSES.RIPPLE);
  for (let i = 0; i < rippleElements.length; i++) {
    rippleElements[i].addEventListener('mousedown', ripple);
  }
}


/**
 * Disables the focus outline for |element| on mousedown.
 * @param {Element} element The element to remove the focus outline from.
 */
function disableOutlineOnMouseClick(element) {
  element.addEventListener('mousedown', (event) => {
    element.classList.add('mouse-navigation');
    let resetOutline = (event) => {
      element.classList.remove('mouse-navigation');
      element.removeEventListener('blur', resetOutline);
    };
    element.addEventListener('blur', resetOutline);
  });
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
    if (event.properyName === 'opacity') {
      hideNotification();
    }
  });

  $(IDS.NOTIFICATION_MESSAGE).textContent =
      configData.translatedStrings.thumbnailRemovedNotification;

  var undoLink = $(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  registerKeyHandler(undoLink, KEYCODE.ENTER, onUndo);
  registerKeyHandler(undoLink, KEYCODE.SPACE, onUndo);
  undoLink.textContent = configData.translatedStrings.undoThumbnailRemove;

  var restoreAllLink = $(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  registerKeyHandler(restoreAllLink, KEYCODE.ENTER, onRestoreAll);
  registerKeyHandler(restoreAllLink, KEYCODE.SPACE, onRestoreAll);
  restoreAllLink.textContent =
      (configData.isCustomLinksEnabled ?
           configData.translatedStrings.restoreDefaultLinks :
           configData.translatedStrings.restoreThumbnailsShort);

  $(IDS.ATTRIBUTION_TEXT).textContent =
      configData.translatedStrings.attributionIntro;

  $(IDS.NOTIFICATION_CLOSE_BUTTON).addEventListener('click', hideNotification);

  var embeddedSearchApiHandle = window.chrome.embeddedSearch;

  ntpApiHandle = embeddedSearchApiHandle.newTabPage;
  ntpApiHandle.onthemechange = onThemeChange;
  ntpApiHandle.onmostvisitedchange = onMostVisitedChange;

  renderTheme();

  var searchboxApiHandle = embeddedSearchApiHandle.searchBox;

  if (configData.isGooglePage) {
    if (configData.isMDIconsEnabled || configData.isCustomLinksEnabled) {
      enableMDIcons();
    } else if (configData.isMDUIEnabled) {
      enableMD();
    }

    if (configData.isCustomLinksEnabled) {
      ntpApiHandle.onaddcustomlinkdone = onAddCustomLinkDone;
      ntpApiHandle.onupdatecustomlinkdone = onUpdateCustomLinkDone;
      ntpApiHandle.ondeletecustomlinkdone = onDeleteCustomLinkDone;
      ntpApiHandle.doesurlresolve = onDoesUrlResolve;
    }

    if (configData.isCustomBackgroundsEnabled ||
        configData.isCustomLinksEnabled) {
      customBackgrounds.init(showErrorNotification, hideNotification);
    }


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
      if (isFakeboxClick(event))
        searchboxApiHandle.startCapturingKeyStrokes();
      else if (isFakeboxFocused())
        searchboxApiHandle.stopCapturingKeyStrokes();
    };
    searchboxApiHandle.onkeycapturechange = function() {
      setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
    };
    var inputbox = $(IDS.FAKEBOX_INPUT);
    inputbox.onpaste = function(event) {
      event.preventDefault();
      // Send pasted text to Omnibox.
      var text = event.clipboardData.getData('text/plain');
      if (text)
        searchboxApiHandle.paste(text);
    };
    inputbox.ondrop = function(event) {
      event.preventDefault();
      var text = event.dataTransfer.getData('text/plain');
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
    disableOutlineOnMouseClick($(IDS.FAKEBOX_MICROPHONE));

    // Update the fakebox style to match the current key capturing state.
    setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
    // Also tell the browser that we're capturing, otherwise it's possible that
    // both fakebox and Omnibox have visible focus at the same time, see
    // crbug.com/792850.
    if (searchboxApiHandle.isKeyCaptureEnabled) {
      searchboxApiHandle.startCapturingKeyStrokes();
    }

    // Load the Doodle. After the first request completes (getting cached
    // data), issue a second request for fresh Doodle data.
    loadDoodle(/*v=*/null, function(ddl) {
      if (ddl === null) {
        // Got no ddl object at all, the feature is probably disabled. Just show
        // the logo.
        showLogoOrDoodle(/*fromCache=*/true);
        return;
      }

      // Got a (possibly empty) ddl object. Show logo or doodle.
      targetDoodle.image = ddl.image || null;
      targetDoodle.metadata = ddl.metadata || null;
      showLogoOrDoodle(/*fromCache=*/true);
      // Never hide an interactive doodle if it was already shown.
      if (ddl.metadata && (ddl.metadata.type === LOGO_TYPE.INTERACTIVE))
        return;
      // If we got a valid ddl object (from cache), load a fresh one.
      if (ddl.v !== null) {
        loadDoodle(ddl.v, function(ddl2) {
          if (ddl2.usable) {
            targetDoodle.image = ddl2.image || null;
            targetDoodle.metadata = ddl2.metadata || null;
            fadeToLogoOrDoodle();
          }
        });
      }
    });

    // Set up doodle notifier (but it may be invisible).
    var doodleNotifier = $(IDS.LOGO_DOODLE_NOTIFIER);
    doodleNotifier.title = configData.translatedStrings.clickToViewDoodle;
    doodleNotifier.addEventListener('click', function(e) {
      e.preventDefault();
      var state = window.history.state || {};
      state.notheme = true;
      window.history.replaceState(state, document.title);
      onThemeChange();
      if (e.detail === 0) {  // Activated by keyboard.
        $(IDS.LOGO_DOODLE_BUTTON).focus();
      }
    });
  } else {
    document.body.classList.add(CLASSES.NON_GOOGLE_PAGE);
  }

  if (searchboxApiHandle.rtl) {
    $(IDS.NOTIFICATION).dir = 'rtl';
    // Grabbing the root HTML element.
    document.documentElement.setAttribute('dir', 'rtl');
    // Add class for setting alignments based on language directionality.
    document.documentElement.classList.add(CLASSES.RTL);
  }

  if (!iframesAndVoiceSearchDisabledForTesting) {
    createIframes();
  }

  document.body.classList.add(CLASSES.INITED);
}


/**
 * Create the Most Visited and edit custom links iframes.
 */
function createIframes() {
  // Collect arguments for the most visited iframe.
  var args = [];

  var searchboxApiHandle = window.chrome.embeddedSearch.searchBox;

  if (searchboxApiHandle.rtl)
    args.push('rtl=1');
  if (NTP_DESIGN.numTitleLines > 1)
    args.push('ntl=' + NTP_DESIGN.numTitleLines);

  args.push(
      'title=' +
      encodeURIComponent(configData.translatedStrings.mostVisitedTitle));
  args.push('removeTooltip=' +
      encodeURIComponent(configData.translatedStrings.removeThumbnailTooltip));

  if (configData.isMDIconsEnabled) {
    args.push('enableMD=1');
  }

  if (configData.isCustomLinksEnabled) {
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
  var iframe = document.createElement('iframe');
  iframe.id = IDS.TILES_IFRAME;
  iframe.name = IDS.TILES_IFRAME;
  iframe.title = configData.translatedStrings.mostVisitedTitle;
  iframe.src = 'chrome-search://most-visited/single.html?' + args.join('&');
  $(IDS.TILES).appendChild(iframe);

  iframe.onload = function() {
    reloadTiles();
    sendThemeInfoToMostVisitedIframe();
  };

  if (configData.isCustomLinksEnabled) {
    // Collect arguments for the edit custom link iframe.
    let clArgs = [];

    if (searchboxApiHandle.rtl)
      clArgs.push('rtl=1');

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
    let clIframe = document.createElement('iframe');
    clIframe.id = IDS.CUSTOM_LINKS_EDIT_IFRAME;
    clIframe.name = IDS.CUSTOM_LINKS_EDIT_IFRAME;
    clIframe.title = configData.translatedStrings.editLinkTitle;
    clIframe.src = 'chrome-search://most-visited/edit.html?' + clArgs.join('&');
    let clIframeDialog = document.createElement('dialog');
    clIframeDialog.id = IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG;
    clIframeDialog.appendChild(clIframe);
    document.body.appendChild(clIframeDialog);
  }

  window.addEventListener('message', handlePostMessage);
}


/**
 * Binds event listeners.
 */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}


/**
 * Injects the One Google Bar into the page. Called asynchronously, so that it
 * doesn't block the main page load.
 */
function injectOneGoogleBar(ogb) {
  var inHeadStyle = document.createElement('style');
  inHeadStyle.type = 'text/css';
  inHeadStyle.appendChild(document.createTextNode(ogb.inHeadStyle));
  document.head.appendChild(inHeadStyle);

  var inHeadScript = document.createElement('script');
  inHeadScript.type = 'text/javascript';
  inHeadScript.appendChild(document.createTextNode(ogb.inHeadScript));
  document.head.appendChild(inHeadScript);

  renderOneGoogleBarTheme();

  var ogElem = $('one-google');
  ogElem.innerHTML = ogb.barHtml;
  ogElem.classList.remove('hidden');

  var afterBarScript = document.createElement('script');
  afterBarScript.type = 'text/javascript';
  afterBarScript.appendChild(document.createTextNode(ogb.afterBarScript));
  ogElem.parentNode.insertBefore(afterBarScript, ogElem.nextSibling);

  $('one-google-end-of-body').innerHTML = ogb.endOfBodyHtml;

  var endOfBodyScript = document.createElement('script');
  endOfBodyScript.type = 'text/javascript';
  endOfBodyScript.appendChild(document.createTextNode(ogb.endOfBodyScript));
  document.body.appendChild(endOfBodyScript);

  ntpApiHandle.logEvent(LOG_TYPE.NTP_ONE_GOOGLE_BAR_SHOWN);
}


/** Loads the Doodle. On success, the loaded script declares a global variable
 * ddl, which onload() receives as its single argument. On failure, onload() is
 * called with null as the argument. If v is null, then the call requests a
 * cached logo. If non-null, it must be the ddl.v of a previous request for a
 * cached logo, and the corresponding fresh logo is returned.
 * @param {?number} v
 * @param {function(?{v, usable, image, metadata})} onload
 */
var loadDoodle = function(v, onload) {
  var ddlScript = document.createElement('script');
  ddlScript.src = 'chrome-search://local-ntp/doodle.js';
  if (v !== null)
    ddlScript.src += '?v=' + v;
  ddlScript.onload = function() {
    onload(ddl);
  };
  ddlScript.onerror = function() {
    onload(null);
  };
  // TODO(treib,sfiera): Add a timeout in case something goes wrong?
  document.body.appendChild(ddlScript);
};


/** Handles the response of a doodle impression ping, i.e. stores the
 * appropriate interactionLogUrl or onClickUrlExtraParams.
 *
 * @param {!Object} ddllog Response object from the ddllog ping.
 * @param {!boolean} isAnimated
 */
var handleDdllogResponse = function(ddllog, isAnimated) {
  if (ddllog && ddllog.interaction_log_url) {
    let interactionLogUrl =
        new URL(ddllog.interaction_log_url, configData.googleBaseUrl);
    if (isAnimated) {
      targetDoodle.animatedInteractionLogUrl = interactionLogUrl;
    } else {
      targetDoodle.staticInteractionLogUrl = interactionLogUrl;
    }
    lastDdllogResponse = 'interaction_log_url ' + ddllog.interaction_log_url;
  } else if (ddllog && ddllog.target_url_params) {
    targetDoodle.onClickUrlExtraParams =
        new URLSearchParams(ddllog.target_url_params);
    lastDdllogResponse = 'target_url_params ' + ddllog.target_url_params;
  } else {
    console.log('Invalid or missing ddllog response:');
    console.log(ddllog);
  }
};


/** Logs a doodle impression at the given logUrl, and handles the response via
 * handleDdllogResponse.
 *
 * @param {!string} logUrl
 * @param {!boolean} isAnimated
 */
var logDoodleImpression = function(logUrl, isAnimated) {
  lastDdllogResponse = '';
  fetch(logUrl, {credentials: 'omit'})
      .then(function(response) {
        return response.text();
      })
      .then(function(text) {
        // Remove the optional XSS preamble.
        const preamble = ')]}\'';
        if (text.startsWith(preamble)) {
          text = text.substr(preamble.length);
        }
        try {
          var json = JSON.parse(text);
        } catch (error) {
          console.log('Failed to parse doodle impression response as JSON:');
          console.log(error);
          return;
        }
        handleDdllogResponse(json.ddllog, isAnimated);
      })
      .catch(function(error) {
        console.log('Error logging doodle impression to "' + logUrl + '":');
        console.log(error);
      })
      .finally(function() {
        ++numDdllogResponsesReceived;
        if (onDdllogResponse !== null) {
          onDdllogResponse();
        }
      });
};


/** Returns true if the target doodle is currently visible. If |image| is null,
 * returns true when the default logo is visible; if non-null, checks that it
 * matches the doodle that is currently visible. Here, "visible" means
 * fully-visible or fading in.
 *
 * @returns {boolean}
 */
var isDoodleCurrentlyVisible = function() {
  var haveDoodle = ($(IDS.LOGO_DOODLE).classList.contains(CLASSES.SHOW_LOGO));
  var wantDoodle =
      (targetDoodle.image !== null) && (targetDoodle.metadata !== null);
  if (!haveDoodle || !wantDoodle) {
    return haveDoodle === wantDoodle;
  }

  // Have a visible doodle and a target doodle. Test that they match.
  if (targetDoodle.metadata.type === LOGO_TYPE.INTERACTIVE) {
    var logoDoodleIframe = $(IDS.LOGO_DOODLE_IFRAME);
    return logoDoodleIframe.classList.contains(CLASSES.SHOW_LOGO) &&
        (logoDoodleIframe.src === targetDoodle.metadata.fullPageUrl);
  } else {
    var logoDoodleImage = $(IDS.LOGO_DOODLE_IMAGE);
    var logoDoodleButton = $(IDS.LOGO_DOODLE_BUTTON);
    return logoDoodleButton.classList.contains(CLASSES.SHOW_LOGO) &&
        ((logoDoodleImage.src === targetDoodle.image) ||
         (logoDoodleImage.src === targetDoodle.metadata.animatedUrl));
  }
};


/** The image and metadata that should be shown, according to the latest fetch.
 * After a logo fades out, onDoodleFadeOutComplete fades in a logo according to
 * targetDoodle.
 */
var targetDoodle = {
  image: null,
  metadata: null,
  // The log URLs and params may be filled with the response from the
  // corresponding impression log URL.
  staticInteractionLogUrl: null,
  animatedInteractionLogUrl: null,
  onClickUrlExtraParams: null,
};


var getDoodleTargetUrl = function() {
  let url = new URL(targetDoodle.metadata.onClickUrl);
  if (targetDoodle.onClickUrlExtraParams) {
    for (var param of targetDoodle.onClickUrlExtraParams) {
      url.searchParams.append(param[0], param[1]);
    }
  }
  return url;
};


var showLogoOrDoodle = function(fromCache) {
  const cachedInteractiveOffline = fromCache &&
      targetDoodle.metadata !== null &&
      targetDoodle.metadata.type == LOGO_TYPE.INTERACTIVE &&
      !window.navigator.onLine;
  if (targetDoodle.metadata !== null && !cachedInteractiveOffline) {
    applyDoodleMetadata();
    if (targetDoodle.metadata.type === LOGO_TYPE.INTERACTIVE) {
      $(IDS.LOGO_DOODLE_BUTTON).classList.remove(CLASSES.SHOW_LOGO);
      $(IDS.LOGO_DOODLE_IFRAME).classList.add(CLASSES.SHOW_LOGO);
    } else {
      $(IDS.LOGO_DOODLE_IMAGE).src = targetDoodle.image;
      $(IDS.LOGO_DOODLE_BUTTON).classList.add(CLASSES.SHOW_LOGO);
      $(IDS.LOGO_DOODLE_IFRAME).classList.remove(CLASSES.SHOW_LOGO);

      // Log the impression in Chrome metrics.
      var isCta = !!targetDoodle.metadata.animatedUrl;
      var eventType = isCta ?
          (fromCache ? LOG_TYPE.NTP_CTA_LOGO_SHOWN_FROM_CACHE :
                       LOG_TYPE.NTP_CTA_LOGO_SHOWN_FRESH) :
          (fromCache ? LOG_TYPE.NTP_STATIC_LOGO_SHOWN_FROM_CACHE :
                       LOG_TYPE.NTP_STATIC_LOGO_SHOWN_FRESH);
      ntpApiHandle.logEvent(eventType);

      // Ping the proper impression logging URL if it exists.
      var logUrl = isCta ? targetDoodle.metadata.ctaLogUrl :
                           targetDoodle.metadata.logUrl;
      if (logUrl) {
        logDoodleImpression(logUrl, /*isAnimated=*/false);
      }
    }
    $(IDS.LOGO_DOODLE).classList.add(CLASSES.SHOW_LOGO);
  } else {
    // No doodle. Just show the default logo.
    $(IDS.LOGO_DEFAULT).classList.add(CLASSES.SHOW_LOGO);
  }
};


/**
 * Starts fading out the given element, which should be either the default logo
 * or the doodle.
 *
 * @param {HTMLElement} element
 */
var startFadeOut = function(element) {
  if (!element.classList.contains(CLASSES.SHOW_LOGO)) {
    return;
  }

  // Compute style now, to ensure that the transition from 1 -> 0 is properly
  // recognized. Otherwise, if a 0 -> 1 -> 0 transition is too fast, the
  // element might stay invisible instead of appearing then fading out.
  window.getComputedStyle(element).opacity;

  element.classList.add(CLASSES.FADE);
  element.classList.remove(CLASSES.SHOW_LOGO);
  element.addEventListener('transitionend', onDoodleFadeOutComplete);
};


/**
 * Integrates a fresh doodle into the page as appropriate. If the correct logo
 * or doodle is already shown, just updates the metadata. Otherwise, initiates
 * a fade from the currently-shown logo/doodle to the new one.
 */
var fadeToLogoOrDoodle = function() {
  // If the image is already visible, there's no need to start a fade-out.
  // However, metadata may have changed, so update the doodle's alt text and
  // href, if applicable.
  if (isDoodleCurrentlyVisible()) {
    if (targetDoodle.metadata !== null) {
      applyDoodleMetadata();
    }
    return;
  }

  // It's not the same doodle. Clear any loging URLs/params we might have.
  targetDoodle.staticInteractionLogUrl = null;
  targetDoodle.animatedInteractionLogUrl = null;
  targetDoodle.onClickUrlExtraParams = null;

  // Start fading out the current logo or doodle. onDoodleFadeOutComplete will
  // apply the change when the fade-out finishes.
  startFadeOut($(IDS.LOGO_DEFAULT));
  startFadeOut($(IDS.LOGO_DOODLE));
};


var onDoodleFadeOutComplete = function(e) {
  // Fade-out finished. Start fading in the appropriate logo.
  $(IDS.LOGO_DOODLE).classList.add(CLASSES.FADE);
  $(IDS.LOGO_DEFAULT).classList.add(CLASSES.FADE);
  showLogoOrDoodle(/*fromCache=*/false);

  this.removeEventListener('transitionend', onDoodleFadeOutComplete);
};


var applyDoodleMetadata = function() {
  var logoDoodleButton = $(IDS.LOGO_DOODLE_BUTTON);
  var logoDoodleImage = $(IDS.LOGO_DOODLE_IMAGE);
  var logoDoodleIframe = $(IDS.LOGO_DOODLE_IFRAME);

  switch (targetDoodle.metadata.type) {
    case LOGO_TYPE.SIMPLE:
      logoDoodleImage.title = targetDoodle.metadata.altText;

      // On click, navigate to the target URL.
      logoDoodleButton.onclick = function() {
        // Log the click in Chrome metrics.
        ntpApiHandle.logEvent(LOG_TYPE.NTP_STATIC_LOGO_CLICKED);

        // Ping the static interaction_log_url if there is one.
        if (targetDoodle.staticInteractionLogUrl) {
          navigator.sendBeacon(targetDoodle.staticInteractionLogUrl);
          targetDoodle.staticInteractionLogUrl = null;
        }

        window.location = getDoodleTargetUrl();
      };
      break;

    case LOGO_TYPE.ANIMATED:
      logoDoodleImage.title = targetDoodle.metadata.altText;
      // The CTA image is currently shown; on click, show the animated one.
      logoDoodleButton.onclick = function(e) {
        e.preventDefault();

        // Log the click in Chrome metrics.
        ntpApiHandle.logEvent(LOG_TYPE.NTP_CTA_LOGO_CLICKED);

        // Ping the static interaction_log_url if there is one.
        if (targetDoodle.staticInteractionLogUrl) {
          navigator.sendBeacon(targetDoodle.staticInteractionLogUrl);
          targetDoodle.staticInteractionLogUrl = null;
        }

        // Once the animated image loads, ping the impression log URL.
        if (targetDoodle.metadata.logUrl) {
          logoDoodleImage.onload = function() {
            logDoodleImpression(
                targetDoodle.metadata.logUrl, /*isAnimated=*/true);
          };
        }
        logoDoodleImage.src = targetDoodle.metadata.animatedUrl;

        // When the animated image is clicked, navigate to the target URL.
        logoDoodleButton.onclick = function() {
          // Log the click in Chrome metrics.
          ntpApiHandle.logEvent(LOG_TYPE.NTP_ANIMATED_LOGO_CLICKED);

          // Ping the animated interaction_log_url if there is one.
          if (targetDoodle.animatedInteractionLogUrl) {
            navigator.sendBeacon(targetDoodle.animatedInteractionLogUrl);
            targetDoodle.animatedInteractionLogUrl = null;
          }

          window.location = getDoodleTargetUrl();
        };
      };
      break;

    case LOGO_TYPE.INTERACTIVE:
      logoDoodleIframe.title = targetDoodle.metadata.altText;
      logoDoodleIframe.src = targetDoodle.metadata.fullPageUrl;
      document.body.style.setProperty(
          '--logo-iframe-width', targetDoodle.metadata.iframeWidthPx + 'px');
      document.body.style.setProperty(
          '--logo-iframe-height', targetDoodle.metadata.iframeHeightPx + 'px');
      document.body.style.setProperty(
          '--logo-iframe-initial-height',
          targetDoodle.metadata.iframeHeightPx + 'px');
      break;
  }
};


return {
  init: init,  // Exposed for testing.
  listen: listen,
  disableIframesAndVoiceSearchForTesting: disableIframesAndVoiceSearchForTesting
};

}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
