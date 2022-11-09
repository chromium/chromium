// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="chromeos_ash">
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// </if>

export interface DownloadsBrowserProxy {
  initializeDownloads(): void;


  selectDownloadLocation(): void;

  resetAutoOpenFileTypes(): void;

  // <if expr="chromeos_ash">
  /**
   * @param path Path to sanitze.
   * @return string to display in UI.
   */
  getDownloadLocationText(path: string): Promise<string>;
  // </if>
}

export class DownloadsBrowserProxyImpl implements DownloadsBrowserProxy {
  initializeDownloads() {
    chrome.send('initializeDownloads');
  }

  selectDownloadLocation() {
    chrome.send('selectDownloadLocation');
  }

  resetAutoOpenFileTypes() {
    chrome.send('resetAutoOpenFileTypes');
  }

  // <if expr="chromeos_ash">
  getDownloadLocationText(path: string) {
    return sendWithPromise('getDownloadLocationText', path);
  }
  // </if>

  static getInstance(): DownloadsBrowserProxy {
    return instance || (instance = new DownloadsBrowserProxyImpl());
  }

  static setInstance(obj: DownloadsBrowserProxy) {
    instance = obj;
  }
}

let instance: DownloadsBrowserProxy|null = null;
