// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

export type StreamInfoWithExtras = chrome.mimeHandlerPrivate.StreamInfo&{
  // Appended in main.js
  javascript?: 'allow' | 'block',
  // Appended in browser_api.js
  tabUrl?: string,
};

/**
 * @param streamInfo The stream object pointing to the data contained in the
 *     PDF.
 * @return A promise that will resolve to the default zoom factor.
 */
function lookupDefaultZoom(streamInfo: chrome.mimeHandlerPrivate.StreamInfo):
    Promise<number> {
  // Webviews don't run in tabs so |streamInfo.tabId| is -1 when running within
  // a webview.
  if (!chrome.tabs || streamInfo.tabId < 0) {
    return Promise.resolve(1);
  }

  return new Promise(function(resolve) {
    chrome.tabs.getZoomSettings(streamInfo.tabId, function(zoomSettings) {
      resolve(zoomSettings.defaultZoomFactor!);
    });
  });
}

/**
 * Returns a promise that will resolve to the initial zoom factor
 * upon starting the plugin. This may differ from the default zoom
 * if, for example, the page is zoomed before the plugin is run.
 * @param streamInfo The stream object pointing to the data contained in the
 *     PDF.
 * @return A promise that will resolve to the initial zoom factor.
 */
function lookupInitialZoom(streamInfo: chrome.mimeHandlerPrivate.StreamInfo):
    Promise<number> {
  // Webviews don't run in tabs so |streamInfo.tabId| is -1 when running within
  // a webview.
  if (!chrome.tabs || streamInfo.tabId < 0) {
    return Promise.resolve(1);
  }

  return new Promise(function(resolve) {
    chrome.tabs.getZoom(streamInfo.tabId, resolve);
  });
}

// A class providing an interface to the browser.
export class BrowserApi {
  private streamInfo_: StreamInfoWithExtras;
  private defaultZoom_: number;
  private initialZoom_: number;
  private zoomBehavior_: ZoomBehavior;

  /**
   * @param streamInfo The stream object pointing to the data contained in the
   *     PDF.
   */
  constructor(
      streamInfo: StreamInfoWithExtras, defaultZoom: number,
      initialZoom: number, zoomBehavior: ZoomBehavior) {
    this.streamInfo_ = streamInfo;
    this.defaultZoom_ = defaultZoom;
    this.initialZoom_ = initialZoom;
    this.zoomBehavior_ = zoomBehavior;
  }

  /**
   * @param streamInfo The stream object pointing to the data contained in the
   *     PDF.
   */
  static create(streamInfo: StreamInfoWithExtras, zoomBehavior: ZoomBehavior):
      Promise<BrowserApi> {
    return Promise
        .all([lookupDefaultZoom(streamInfo), lookupInitialZoom(streamInfo)])
        .then(function(zoomFactors) {
          return new BrowserApi(
              streamInfo, zoomFactors[0], zoomFactors[1], zoomBehavior);
        });
  }

  /**
   * @return The stream object pointing to the data contained in the PDF.
   */
  getStreamInfo(): StreamInfoWithExtras {
    return this.streamInfo_;
  }

  /**
   * Navigates the current tab.
   * @param url The URL to navigate the tab to.
   */
  navigateInCurrentTab(url: string) {
    const tabId = this.getStreamInfo().tabId;
    // We need to use the tabs API to navigate because
    // |window.location.href| cannot be used. This PDF extension is not loaded
    // in the top level frame (it's embedded using MimeHandlerView). Using
    // |window.location| would navigate the wrong frame, so we can't
    // use it as a fallback. If it turns out that we do need a way to navigate
    // in non-tab cases, we would need to create another mechanism to
    // communicate with MimeHandler code in the browser (e.g. via
    // mimeHandlerPrivate), which could then navigate the correct frame.
    // Furthermore, navigations to local resources would be blocked with
    // |window.location|.
    if (chrome.tabs && tabId !== chrome.tabs.TAB_ID_NONE) {
      chrome.tabs.update(tabId, {url: url});
    }
  }

  /**
   * Sets the browser zoom.
   * @param zoom The zoom factor to send to the browser.
   * @return A promise that will be resolved when the browser zoom has been
   *     updated.
   */
  setZoom(zoom: number): Promise<void> {
    assert(
        this.zoomBehavior_ === ZoomBehavior.MANAGE,
        'Viewer does not manage browser zoom.');
    return new Promise(resolve => {
      chrome.tabs.setZoom(this.streamInfo_.tabId, zoom, resolve);
    });
  }

  /** @return The default browser zoom factor. */
  getDefaultZoom(): number {
    return this.defaultZoom_;
  }

  /** @return The initial browser zoom factor. */
  getInitialZoom(): number {
    return this.initialZoom_;
  }

  /** @return How to manage zoom. */
  getZoomBehavior(): ZoomBehavior {
    return this.zoomBehavior_;
  }

  /**
   * Adds an event listener to be notified when the browser zoom changes.
   *
   * @param listener The listener to be called with the new zoom factor.
   */
  addZoomEventListener(listener: (newZoom: number) => void) {
    if (!(this.zoomBehavior_ === ZoomBehavior.MANAGE ||
          this.zoomBehavior_ === ZoomBehavior.PROPAGATE_PARENT)) {
      return;
    }

    chrome.tabs.onZoomChange.addListener(info => {
      if (info.tabId !== this.streamInfo_.tabId) {
        return;
      }
      listener(info.newZoomFactor);
    });
  }
}

/** Enumeration of ways to manage zoom changes. */
export enum ZoomBehavior {
  NONE = 0,
  MANAGE = 1,
  PROPAGATE_PARENT = 2,
}

/**
 * Creates a BrowserApi for an extension running as a mime handler.
 * @return A promise to a BrowserApi instance. The instance is constructed by
 *     the pdfViewerPrivate API if PDF OOPIF is enabled, otherwise it is
 *     constructed by the mimeHandlerPrivate API.
 */
export function createBrowserApi(): Promise<BrowserApi> {
  return new Promise<chrome.mimeHandlerPrivate.StreamInfo>(function(resolve) {
           if (document.documentElement.hasAttribute('pdfOopifEnabled')) {
             chrome.pdfViewerPrivate.getStreamInfo(resolve);
           } else {
             chrome.mimeHandlerPrivate.getStreamInfo(resolve);
           }
         })
      .then(function(streamInfo) {
        const promises = [];
        let zoomBehavior = ZoomBehavior.NONE;
        if (streamInfo.tabId !== -1) {
          zoomBehavior = streamInfo.embedded ? ZoomBehavior.PROPAGATE_PARENT :
                                               ZoomBehavior.MANAGE;
          promises.push(
              new Promise<chrome.tabs.Tab|undefined>(function(resolve) {
                chrome.tabs.get(streamInfo.tabId, resolve);
              }).then(function(tab) {
                if (tab) {
                  (streamInfo as StreamInfoWithExtras).tabUrl = tab!.url;
                }
              }));
        }
        if (zoomBehavior === ZoomBehavior.MANAGE) {
          promises.push(new Promise<void>(function(resolve) {
            chrome.tabs.setZoomSettings(
                streamInfo.tabId, {
                  mode: chrome.tabs.ZoomSettingsMode.MANUAL,
                  scope: chrome.tabs.ZoomSettingsScope.PER_TAB,
                },
                resolve);
          }));
        }
        return Promise.all(promises).then(function() {
          return BrowserApi.create(
              streamInfo as StreamInfoWithExtras, zoomBehavior);
        });
      });
}

/**
 * Creates a BrowserApi instance for an extension not running as a mime handler.
 * @return A promise to a BrowserApi instance constructed from the URL.
 */
export function createBrowserApiForPrintPreview(): Promise<BrowserApi> {
  const url = window.location.search.substring(1);
  const streamInfo: StreamInfoWithExtras = {
    streamUrl: url,
    originalUrl: url,
    responseHeaders: {},
    embedded: window.parent !== window,
    tabId: -1,
    mimeType: '',
  };
  return new Promise<void>(function(resolve) {
           if (!chrome.tabs) {
             resolve();
             return;
           }
           chrome.tabs.getCurrent(function(tab) {
             streamInfo.tabId = tab!.id!;
             streamInfo.tabUrl = tab!.url;
             resolve();
           });
         })
      .then(function() {
        return BrowserApi.create(streamInfo, ZoomBehavior.NONE);
      });
}
