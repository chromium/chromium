// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SpokenMessage} from '../components/spoken-message.js';

import {i18n} from './i18n.js';

/**
 * Move the focus back to the beginning of body, and announce the change for
 * screen reader.
 *
 * This is implemented by creating a <spoken-message> at the beginning of the
 * body, focus on it, and remove it afterwards.
 */
export function focusToBody(): void {
  const message = new SpokenMessage();
  message.ariaLive = 'polite';
  message.tabIndex = -1;
  message.textContent = i18n.appName;
  document.body.prepend(message);
  message.focus();
  // ChromeVox don't speaks the message if we immediately remove it, so
  // schedule the removal at the next macrotask.
  setTimeout(() => {
    message.remove();
  });
}

// TODO(pihsun): Add helper functions for other focus management related
// functions, like autofocus on firstUpdate.
