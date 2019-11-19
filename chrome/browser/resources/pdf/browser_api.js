// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * @param {!Object} streamInfo The stream object pointing to the data contained
 *     in the PDF.
 * @return {Promise<number>} A promise that will resolve to the default zoom
 *     factor.
 */
function lookupDefaultZoom(streamInfo) {
  // Webviews don't run in tabs so |streamInfo.tabId| is -1 when running within
  // a webview.
  if (!chrome.tabs || streamInfo.tabId < 0) {
    return Promise.resolve(1);
  }

  return new Promise(function(resolve, reject) {
    chrome.tabs.getZoomSettings(streamInfo.tabId, function(zoomSettings) {
      resolve(zoomSettings.defaultZoomFactor);
    });
  });
}

/**
 * Returns a promise that will resolve to the initial zoom factor
 * upon starting the plugin. This may differ from the default zoom
 * if, for example, the page is zoomed before the plugin is run.
 *
 * @param {!Object} streamInfo The stream object pointing to the data contained
 *     in the PDF.
 * @return {Promise<number>} A promise that will resolve to the initial zoom
 *     factor.
 */
function lookupInitialZoom(streamInfo) {
  // Webviews don't run in tabs so |streamInfo.tabId| is -1 when running within
  // a webview.
  if (!chrome.tabs || streamInfo.tabId < 0) {
    return Promise.resolve(1);
  }

  return new Promise(function(resolve, reject) {
    chrome.tabs.getZoom(streamInfo.tabId, resolve);
  });
}

/**
 * A class providing an interface to the browser.
 */
export class BrowserApi {
  /**
   * @param {!Object} streamInfo The stream object which points to the data
   *     contained in the PDF.
   * @param {number} defaultZoom The default browser zoom.
   * @param {number} initialZoom The initial browser zoom
   *     upon starting the plugin.
   * @param {BrowserApi.ZoomBehavior} zoomBehavior How to manage zoom.
   */
  constructor(streamInfo, defaultZoom, initialZoom, zoomBehavior) {
    this.streamInfo_ = streamInfo;
    this.defaultZoom_ = defaultZoom;
    this.initialZoom_ = initialZoom;
    this.zoomBehavior_ = zoomBehavior;
  }

  /**
   * @param {!Object} streamInfo The stream object pointing to the data
   *     contained in the PDF.
   * @param {BrowserApi.ZoomBehavior} zoomBehavior How to manage zoom.
   * @return {Promise<BrowserApi>} A promise to a BrowserApi.
   */
  static create(streamInfo, zoomBehavior) {
    return Promise
        .all([lookupDefaultZoom(streamInfo), lookupInitialZoom(streamInfo)])
        .then(function(zoomFactors) {
          return new BrowserApi(
              streamInfo, zoomFactors[0], zoomFactors[1], zoomBehavior);
        });
  }

  /**
   * @return {Object} The stream info object pointing to the data contained in
   *     the PDF.
   */
  getStreamInfo() {
    return this.streamInfo_;
  }

  /**
   * Sets the browser zoom.
   *
   * @param {number} zoom The zoom factor to send to the browser.
   * @return {Promise} A promise that will be resolved when the browser zoom
   *     has been updated.
   */
  setZoom(zoom) {
    assert(
        this.zoomBehavior_ == BrowserApi.ZoomBehavior.MANAGE,
        'Viewer does not manage browser zoom.');
    return new Promise((resolve, reject) => {
      chrome.tabs.setZoom(this.streamInfo_.tabId, zoom, resolve);
    });
  }

  /**
   * @return {number} The default browser zoom factor.
   */
  getDefaultZoom() {
    return this.defaultZoom_;
  }

  /**
   * @return {number} The initial browser zoom factor.
   */
  getInitialZoom() {
    return this.initialZoom_;
  }

  /**
   * @return {BrowserApi.ZoomBehavior} How to manage zoom.
   */
  getZoomBehavior() {
    return this.zoomBehavior_;
  }

  /**
   * Adds an event listener to be notified when the browser zoom changes.
   *
   * @param {!Function} listener The listener to be called with the new zoom
   *     factor.
   */
  addZoomEventListener(listener) {
    if (!(this.zoomBehavior_ == BrowserApi.ZoomBehavior.MANAGE ||
          this.zoomBehavior_ == BrowserApi.ZoomBehavior.PROPAGATE_PARENT)) {
      return;
    }

    chrome.tabs.onZoomChange.addListener(info => {
      const zoomChangeInfo =
          /** @type {{tabId: number, newZoomFactor: number}} */ (info);
      if (zoomChangeInfo.tabId != this.streamInfo_.tabId) {
        return;
      }
      listener(zoomChangeInfo.newZoomFactor);
    });
  }
}

/**
 * Enumeration of ways to manage zoom changes.
 * @enum {number}
 */
BrowserApi.ZoomBehavior = {
  NONE: 0,
  MANAGE: 1,
  PROPAGATE_PARENT: 2
};

/**
 * Creates a BrowserApi for an extension running as a mime handler.
 *
 * @return {!Promise<!BrowserApi>} A promise to a BrowserApi instance
 *     constructed using the mimeHandlerPrivate API.
 */
function createBrowserApiForMimeHandlerView() {
  return new Promise(function(resolve, reject) {
           chrome.mimeHandlerPrivate.getStreamInfo(resolve);
         })
      .then(function(streamInfo) {
        const promises = [];
        let zoomBehavior = BrowserApi.ZoomBehavior.NONE;
        if (streamInfo.tabId != -1) {
          zoomBehavior = streamInfo.embedded ?
              BrowserApi.ZoomBehavior.PROPAGATE_PARENT :
              BrowserApi.ZoomBehavior.MANAGE;
          promises.push(new Promise(function(resolve) {
                          chrome.tabs.get(streamInfo.tabId, resolve);
                        }).then(function(tab) {
            if (tab) {
              streamInfo.tabUrl = tab.url;
            }
          }));
        }
        if (zoomBehavior == BrowserApi.ZoomBehavior.MANAGE) {
          promises.push(new Promise(function(resolve) {
            chrome.tabs.setZoomSettings(
                streamInfo.tabId, {mode: 'manual', scope: 'per-tab'}, resolve);
          }));
        }
        return Promise.all(promises).then(function() {
          return BrowserApi.create(streamInfo, zoomBehavior);
        });
      });
}

/**
 * Creates a BrowserApi instance for an extension not running as a mime handler.
 *
 * @return {!Promise<!BrowserApi>} A promise to a BrowserApi instance
 *     constructed from the URL.
 */
function createBrowserApiForPrintPreview() {
  const url = window.location.search.substring(1);
  const streamInfo = {
    streamUrl: url,
    originalUrl: url,
    responseHeaders: {},
    embedded: window.parent != window,
    tabId: -1,
  };
  return new Promise(function(resolve, reject) {
           if (!chrome.tabs) {
             resolve();
             return;
           }
           chrome.tabs.getCurrent(function(tab) {
             streamInfo.tabId = tab.id;
             streamInfo.tabUrl = tab.url;
             resolve();
           });
         })
      .then(function() {
        return BrowserApi.create(streamInfo, BrowserApi.ZoomBehavior.NONE);
      });
}

/**
 * @return {!Promise<!BrowserApi>} A promise to a BrowserApi instance for the
 *     current environment.
 */
export function createBrowserApi() {
  if (location.origin === 'chrome://print') {
    return createBrowserApiForPrintPreview();
  }

  return createBrowserApiForMimeHandlerView();
}
