// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {HelpContentList, HelpContentType, SearchRequest, SearchResponse} from './feedback_types.js';

/**
 * @fileoverview
 * Fake data used for testing purpose.
 */

/** @type {!HelpContentList} */
export const fakeHelpContentList = [
  {
    title: stringToMojoString16('Fix connection problems'),
    url: {url: 'https://support.google.com/chromebook/?q=6318213'},
    content_type: HelpContentType.kArticle
  },
  {
    title: stringToMojoString16(
        'Why won\'t my wireless mouse with a USB piece wor...?'),
    url: {url: 'https://support.google.com/chromebook/?q=123920509'},
    content_type: HelpContentType.kForum
  },
  {
    title: stringToMojoString16('Wifi Issues - only on Chromebooks'),
    url: {url: 'https://support.google.com/chromebook/?q=114174470'},
    content_type: HelpContentType.kForum
  },
  {
    title: stringToMojoString16('Network Connectivity Fault'),
    url: {url: 'https://support.google.com/chromebook/?q=131459420'},
    content_type: HelpContentType.kForum
  },
  {
    title: stringToMojoString16(
        'Connected to WiFi but can\'t connect to the internet'),
    url: {url: 'https://support.google.com/chromebook/?q=22864239'},
    content_type: HelpContentType.kForum
  }
];

/** @type {!SearchRequest} */
export const fakeSearchRequest = {
  maxResults: 5,
  query: stringToMojoString16('wifi not working'),
};

/** @type {!SearchResponse} */
export const fakeSearchResponse = {
  results: fakeHelpContentList,
  totalResults: 10,
};
