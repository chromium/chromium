// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HelpContentList, HelpContentType} from './feedback_types.js';

/**
 * @fileoverview
 * Fake data used for testing purpose.
 */

/** @type {!HelpContentList} */
export const fakeHelpContentList = [
  {
    title: 'Fix connection problems',
    url: 'https://support.google.com/chromebook/?q=6318213',
    content_type: HelpContentType.ARTICLE
  },
  {
    title: 'Why won\'t my wireless mouse with a USB piece wor...?',
    url: 'https://support.google.com/chromebook/?q=123920509',
    content_type: HelpContentType.FORUM
  },
  {
    title: 'Wifi Issues - only on Chromebooks',
    url: 'https://support.google.com/chromebook/?q=114174470',
    content_type: HelpContentType.FORUM
  },
  {
    title: 'Network Connectivity Fault',
    url: 'https://support.google.com/chromebook/?q=131459420',
    content_type: HelpContentType.FORUM
  },
  {
    title: 'Connected to WiFi but can\'t connect to the internet',
    url: 'https://support.google.com/chromebook/?q=22864239',
    content_type: HelpContentType.FORUM
  }
];
