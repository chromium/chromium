// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserApi} from './browser_api.js';
import type {OpenPdfParams, OpenPdfParamsParser} from './open_pdf_params_parser.js';
import type {Viewport} from './viewport.js';

// NavigatorDelegate for calling browser-specific functions to do the actual
// navigating.
export interface NavigatorDelegate {
  /**
   * Called when navigation should happen in the current tab.
   */
  navigateInCurrentTab(url: string): void;

  /**
   * Called when navigation should happen in the new tab.
   * @param active Indicates if the new tab should be the active tab.
   */
  navigateInNewTab(url: string, active: boolean): void;

  /**
   * Called when navigation should happen in the new window.
   */
  navigateInNewWindow(url: string): void;

  /*
   * Returns true if `url` should be allowed to access local files, false
   * otherwise.
   */
  isAllowedLocalFileAccess(url: string): Promise<boolean>;
}

// NavigatorDelegate for calling browser-specific functions to do the actual
// navigating.
export class NavigatorDelegateImpl implements NavigatorDelegate {
  private browserApi_: BrowserApi;

  constructor(browserApi: BrowserApi) {
    this.browserApi_ = browserApi;
  }

  navigateInCurrentTab(url: string) {
    this.browserApi_.navigateInCurrentTab(url);
  }

  navigateInNewTab(url: string, active: boolean) {
    // Prefer the tabs API because it guarantees we can just open a new tab.
    // window.open doesn't have this guarantee.
    if (chrome.tabs) {
      chrome.tabs.create({url: url, active: active});
    } else {
      window.open(url);
    }
  }

  navigateInNewWindow(url: string) {
    // Prefer the windows API because it guarantees we can just open a new
    // window. window.open with '_blank' argument doesn't have this guarantee.
    if (chrome.windows) {
      chrome.windows.create({url: url});
    } else {
      window.open(url, '_blank');
    }
  }


  isAllowedLocalFileAccess(url: string): Promise<boolean> {
    return new Promise(resolve => {
      chrome.pdfViewerPrivate.isAllowedLocalFileAccess(
          url, result => resolve(result));
    });
  }
}

// Navigator for navigating to links inside or outside the PDF.
export class PdfNavigator {
  private originalUrl_: URL|null = null;
  private viewport_: Viewport;
  private paramsParser_: OpenPdfParamsParser;
  private navigatorDelegate_: NavigatorDelegate;

  /**
   * @param originalUrl The original page URL.
   * @param viewport The viewport info of the page.
   * @param paramsParser The object for URL parsing.
   * @param navigatorDelegate The object with callback functions that get called
   *    when navigation happens in the current tab, a new tab, and a new window.
   */
  constructor(
      originalUrl: string, viewport: Viewport,
      paramsParser: OpenPdfParamsParser, navigatorDelegate: NavigatorDelegate) {
    try {
      this.originalUrl_ = new URL(originalUrl);
    } catch (err) {
      console.warn('Invalid original URL');
    }

    this.viewport_ = viewport;
    this.paramsParser_ = paramsParser;
    this.navigatorDelegate_ = navigatorDelegate;
  }

  /**
   * Function to navigate to the given URL. This might involve navigating
   * within the PDF page or opening a new url (in the same tab or a new tab).
   * @param disposition The window open disposition when navigating to the new
   *     URL.
   * @return When navigation has completed (used for testing).
   */
  async navigate(urlString: string, disposition: WindowOpenDisposition):
      Promise<void> {
    if (urlString.length === 0) {
      return Promise.resolve();
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
      urlString = await this.guessUrlWithoutScheme_(urlString);
    }

    let url = null;
    try {
      url = new URL(urlString);
    } catch (err) {
      return Promise.reject(err);
    }

    if (!(await this.isValidUrl_(url))) {
      return Promise.resolve();
    }

    let whenDone = Promise.resolve();

    switch (disposition) {
      case WindowOpenDisposition.CURRENT_TAB:
        whenDone = this.paramsParser_.getViewportFromUrlParams(url.href).then(
            this.onViewportReceived_.bind(this));
        break;
      case WindowOpenDisposition.NEW_BACKGROUND_TAB:
        this.navigatorDelegate_.navigateInNewTab(url.href, false);
        break;
      case WindowOpenDisposition.NEW_FOREGROUND_TAB:
        this.navigatorDelegate_.navigateInNewTab(url.href, true);
        break;
      case WindowOpenDisposition.NEW_WINDOW:
        this.navigatorDelegate_.navigateInNewWindow(url.href);
        break;
      case WindowOpenDisposition.SAVE_TO_DISK:
        // TODO(jaepark): Alt + left clicking a link in PDF should
        // download the link.
        whenDone = this.paramsParser_.getViewportFromUrlParams(url.href).then(
            this.onViewportReceived_.bind(this));
        break;
      default:
        break;
    }

    return whenDone;
  }

  /**
   * Called when the viewport position is received.
   * @param viewportPosition Dictionary containing the viewport
   *    position.
   */
  private onViewportReceived_(viewportPosition: OpenPdfParams) {
    let newUrl = null;
    try {
      newUrl = new URL(viewportPosition.url!);
    } catch (err) {
    }

    const pageNumber = viewportPosition.page;
    if (pageNumber !== undefined && this.originalUrl_ && newUrl &&
        this.originalUrl_.origin === newUrl.origin &&
        this.originalUrl_.pathname === newUrl.pathname) {
      this.viewport_.goToPage(pageNumber);
    } else {
      this.navigatorDelegate_.navigateInCurrentTab(viewportPosition.url!);
    }
  }

  /**
   * Checks if the URL starts with a scheme and is not just a scheme.
   */
  private async isValidUrl_(url: URL): Promise<boolean> {
    // Make sure |url| starts with a valid scheme.
    const validSchemes = ['http:', 'https:', 'ftp:', 'file:', 'mailto:'];
    if (!validSchemes.includes(url.protocol)) {
      return false;
    }

    // Navigations to file:-URLs are only allowed from file:-URLs or allowlisted
    // domains.
    if (url.protocol === 'file:' && this.originalUrl_ &&
        this.originalUrl_.protocol !== 'file:') {
      return this.navigatorDelegate_.isAllowedLocalFileAccess(
          this.originalUrl_.toString());
    }

    return true;
  }

  /**
   * Attempt to figure out what a URL is when there is no scheme.
   * @return The URL with a scheme or the original URL if it is not
   *     possible to determine the scheme.
   */
  private async guessUrlWithoutScheme_(url: string): Promise<string> {
    // If the original URL is mailto:, that does not make sense to start with,
    // and neither does adding |url| to it.
    // If the original URL is not a valid URL, this cannot make a valid URL.
    // In both cases, just bail out.
    if (!this.originalUrl_ || this.originalUrl_.protocol === 'mailto:' ||
        !(await this.isValidUrl_(this.originalUrl_))) {
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
      const domainName = domainSeparatorIndex === -1 ?
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
 */
export enum WindowOpenDisposition {
  CURRENT_TAB = 1,
  NEW_FOREGROUND_TAB = 3,
  NEW_BACKGROUND_TAB = 4,
  NEW_WINDOW = 6,
  SAVE_TO_DISK = 7,
}

// Export on |window| such that scripts injected from pdf_extension_test.cc can
// access it.
Object.assign(window, {PdfNavigator, WindowOpenDisposition});
