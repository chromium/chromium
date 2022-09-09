// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface BrowserProxy {
  textChanged(newText: string): void;
  optionSelected(index: number, resultSetId: number): void;
  heightChanged(newHeight: number): void;
  dismiss(): void;
  promptCancelled(): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  /**
   * Notifies the backend that user input has changed.
   * @param newText The current contents of the user input field.
   */
  textChanged(newText: string) {
    chrome.send('textChanged', [newText]);
  }

  /**
   * Notifies the backend that the option at |index| in result set
   * |resultSetId| was selected.
   * @param index The index of the selected option.
   * @param resultSetId The result set this option was presented in.
   */
  optionSelected(index: number, resultSetId: number) {
    chrome.send('optionSelected', [index, resultSetId]);
  }

  /**
   * Notifies the views layer that the inherent height of the UI has changed
   * so that the window can grow or shrink.
   * @param newHeight The current height of the element.
   */
  heightChanged(newHeight: number) {
    chrome.send('heightChanged', [newHeight]);
  }

  /**
   * Notifies the backend that the user wants to dismiss the UI.
   */
  dismiss() {
    chrome.send('dismiss');
  }

  /**
   * Notifies the backend that the user has cancelled entering a composite
   * command.
   */
  promptCancelled() {
    chrome.send('compositeCommandCancelled');
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
