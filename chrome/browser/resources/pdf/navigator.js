// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OpenPdfParamsParser} from './open_pdf_params_parser.js';
import {Viewport} from './viewport.js';

/**
 * NavigatorDelegate for calling browser-specific functions to do the actual
 * navigating.
 */
export class NavigatorDelegate {
  /**
   * @param {number} tabId The tab ID of the PDF viewer or -1 if the viewer is
   *     not displayed in a tab.
   */
  constructor(tabId) {
    /** @private {number} */
    this.tabId_ = tabId;
  }

  /**
   * Called when navigation should happen in the current tab.
   * @param {string} url The url to be opened in the current tab.
   */
  navigateInCurrentTab(url) {
    // When the PDFviewer is inside a browser tab, prefer the tabs API because
    // it can navigate from one file:// URL to another.
    if (chrome.tabs && this.tabId_ != -1) {
      chrome.tabs.update(this.tabId_, {url: url});
    } else {
      window.location.href = url;
    }
  }

  /**
   * Called when navigation should happen in the new tab.
   * @param {string} url The url to be opened in the new tab.
   * @param {boolean} active Indicates if the new tab should be the active tab.
   */
  navigateInNewTab(url, active) {
    // Prefer the tabs API because it guarantees we can just open a new tab.
    // window.open doesn't have this guarantee.
    if (chrome.tabs) {
      chrome.tabs.create({url: url, active: active});
    } else {
      window.open(url);
    }
  }

  /**
   * Called when navigation should happen in the new window.
   * @param {string} url The url to be opened in the new window.
   */
  navigateInNewWindow(url) {
    // Prefer the windows API because it guarantees we can just open a new
    // window. window.open with '_blank' argument doesn't have this guarantee.
    if (chrome.windows) {
      chrome.windows.create({url: url});
    } else {
      window.open(url, '_blank');
    }
  }
}

/** Navigator for navigating to links inside or outside the PDF. */
export class PdfNavigator {
  /**
   * @param {string} originalUrl The original page URL.
   * @param {!Viewport} viewport The viewport info of the page.
   * @param {!OpenPdfParamsParser} paramsParser The object for URL parsing.
   * @param {!NavigatorDelegate} navigatorDelegate The object with callback
   *    functions that get called when navigation happens in the current tab,
   *    a new tab, and a new window.
   */
  constructor(originalUrl, viewport, paramsParser, navigatorDelegate) {
    /** @private {?URL} */
    this.originalUrl_ = null;
    try {
      this.originalUrl_ = new URL(originalUrl);
    } catch (err) {
      console.warn('Invalid original URL');
    }

    /** @private {!Viewport} */
    this.viewport_ = viewport;

    /** @private {!OpenPdfParamsParser} */
    this.paramsParser_ = paramsParser;

    /** @private {!NavigatorDelegate} */
    this.navigatorDelegate_ = navigatorDelegate;
  }

  /**
   * Function to navigate to the given URL. This might involve navigating
   * within the PDF page or opening a new url (in the same tab or a new tab).
   * @param {string} urlString The URL to navigate to.
   * @param {!PdfNavigator.WindowOpenDisposition} disposition The window open
   *     disposition when navigating to the new URL.
   */
  navigate(urlString, disposition) {
    if (urlString.length == 0) {
      return;
    }

    // If |urlFragment| starts with '#', then it's for the same URL with a
    // different URL fragment.
    if (urlString[0] === '#' && this.originalUrl_) {
      // if '#' is already present in |originalUrl| then remove old fragment
      // and add new url fragment.
      const newUrl = new URL(this.originalUrl_.href);
      newUrl.hash = urlString;
      urlString = newUrl.href;
    }

    // If there's no scheme, then take a guess at the scheme.
    if (!urlString.includes('://') && !urlString.includes('mailto:')) {
      urlString = this.guessUrlWithoutScheme_(urlString);
    }

    let url = null;
    try {
      url = new URL(urlString);
    } catch (err) {
      return;
    }

    if (!this.isValidUrl_(url)) {
      return;
    }

    switch (disposition) {
      case PdfNavigator.WindowOpenDisposition.CURRENT_TAB:
        this.paramsParser_.getViewportFromUrlParams(
            url.href, this.onViewportReceived_.bind(this));
        break;
      case PdfNavigator.WindowOpenDisposition.NEW_BACKGROUND_TAB:
        this.navigatorDelegate_.navigateInNewTab(url.href, false);
        break;
      case PdfNavigator.WindowOpenDisposition.NEW_FOREGROUND_TAB:
        this.navigatorDelegate_.navigateInNewTab(url.href, true);
        break;
      case PdfNavigator.WindowOpenDisposition.NEW_WINDOW:
        this.navigatorDelegate_.navigateInNewWindow(url.href);
        break;
      case PdfNavigator.WindowOpenDisposition.SAVE_TO_DISK:
        // TODO(jaepark): Alt + left clicking a link in PDF should
        // download the link.
        this.paramsParser_.getViewportFromUrlParams(
            url.href, this.onViewportReceived_.bind(this));
        break;
      default:
        break;
    }
  }

  /**
   * Called when the viewport position is received.
   * @param {Object} viewportPosition Dictionary containing the viewport
   *    position.
   * @private
   */
  onViewportReceived_(viewportPosition) {
    let newUrl = null;
    try {
      newUrl = new URL(viewportPosition.url);
    } catch (err) {
    }

    const pageNumber = viewportPosition.page;
    if (pageNumber != undefined && this.originalUrl_ && newUrl &&
        this.originalUrl_.origin === newUrl.origin &&
        this.originalUrl_.pathname === newUrl.pathname) {
      this.viewport_.goToPage(pageNumber);
    } else {
      this.navigatorDelegate_.navigateInCurrentTab(viewportPosition.url);
    }
  }

  /**
   * Checks if the URL starts with a scheme and is not just a scheme.
   * @param {!URL} url The input URL
   * @return {boolean} Whether the url is valid.
   * @private
   */
  isValidUrl_(url) {
    // Make sure |url| starts with a valid scheme.
    const validSchemes = ['http:', 'https:', 'ftp:', 'file:', 'mailto:'];
    if (!validSchemes.includes(url.protocol)) {
      return false;
    }

    // Navigations to file:-URLs are only allowed from file:-URLs.
    if (url.protocol === 'file:' && this.originalUrl_ &&
        this.originalUrl_.protocol !== 'file:') {
      return false;
    }

    return true;
  }

  /**
   * Attempt to figure out what a URL is when there is no scheme.
   * @param {string} url The input URL
   * @return {string} The URL with a scheme or the original URL if it is not
   *     possible to determine the scheme.
   * @private
   */
  guessUrlWithoutScheme_(url) {
    // If the original URL is mailto:, that does not make sense to start with,
    // and neither does adding |url| to it.
    // If the original URL is not a valid URL, this cannot make a valid URL.
    // In both cases, just bail out.
    if (!this.originalUrl_ || this.originalUrl_.protocol === 'mailto:' ||
        !this.isValidUrl_(this.originalUrl_)) {
      return url;
    }

    // Check for absolute paths.
    if (url.startsWith('/')) {
      return this.originalUrl_.origin + url;
    }

    // Check for other non-relative paths.
    // In Adobe Acrobat Reader XI, it looks as though links with less than
    // 2 dot separators in the domain are considered relative links, and
    // those with 2 or more are considered http URLs. e.g.
    //
    // www.foo.com/bar -> http
    // foo.com/bar -> relative link
    if (url.startsWith('\\')) {
      // Prepend so that the relative URL will be correctly computed by new
      // URL() below.
      url = './' + url;
    }
    if (!url.startsWith('.')) {
      const domainSeparatorIndex = url.indexOf('/');
      const domainName = domainSeparatorIndex == -1 ?
          url :
          url.substr(0, domainSeparatorIndex);
      const domainDotCount = (domainName.match(/\./g) || []).length;
      if (domainDotCount >= 2) {
        return 'http://' + url;
      }
    }

    return new URL(url, this.originalUrl_.href).href;
  }
}

/**
 * Represents options when navigating to a new url. C++ counterpart of
 * the enum is in ui/base/window_open_disposition.h. This enum represents
 * the only values that are passed from Plugin.
 * @enum {number}
 */
PdfNavigator.WindowOpenDisposition = {
  CURRENT_TAB: 1,
  NEW_FOREGROUND_TAB: 3,
  NEW_BACKGROUND_TAB: 4,
  NEW_WINDOW: 6,
  SAVE_TO_DISK: 7
};

// Export on |window| such that scripts injected from pdf_extension_test.cc can
// access it.
window.PdfNavigator = PdfNavigator;
