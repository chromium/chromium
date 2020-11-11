// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class BrowserProxy {
  /**
   * Notifies the backend that user input has changed.
   * @param {string} newText The current contents of the user input field.
   */
  textChanged(newText) {}

  /**
   * Notifies the backend that the option at |index| in result set
   * |resultSetId| was selected.
   * @param {number} index The index of the selected option.
   * @param {number} resultSetId The result set this option was presented in.
   */
  optionSelected(index, resultSetId) {}

  /**
   * Notifies the views layer that the inherent height of the UI has changed
   * so that the window can grow or shrink.
   * @param {number} newHeight The current height of the element.
   */
  heightChanged(newHeight) {}

  /**
   * Notifies the backend that the user wants to dismiss the UI.
   */
  dismiss() {}

  /**
   * Notifies the backend that the user has cancelled entering a composite
   * command.
   */
  promptCancelled() {}
}

/** @implements {BrowserProxy} */
export class BrowserProxyImpl {
  /** @override */
  textChanged(newText) {
    chrome.send('textChanged', [newText]);
  }

  /** @override */
  optionSelected(index, resultSetId) {
    chrome.send('optionSelected', [index, resultSetId]);
  }

  /** @override */
  heightChanged(newHeight) {
    chrome.send('heightChanged', [newHeight]);
  }

  /** @override */
  dismiss() {
    chrome.send('dismiss');
  }

  /** @override */
  promptCancelled() {
    chrome.send('compositeCommandCancelled');
  }
}


addSingletonGetter(BrowserProxyImpl);
