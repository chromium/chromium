// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {FeedbackContext, HelpContentList, HelpContentType, SearchRequest, SearchResponse} from './feedback_types.js';

/**
 * @fileoverview
 * Fake data used for testing purpose.
 */

/** @type {!HelpContentList} */
export const fakePopularHelpContentList = [
  {
    title: stringToMojoString16('fake article'),
    url: {url: 'https://support.google.com/chromebook/?q=article'},
    contentType: HelpContentType.kArticle
  },
  {
    title: stringToMojoString16('fake forum'),
    url: {url: 'https://support.google.com/chromebook/?q=forum'},
    contentType: HelpContentType.kForum
  }
];

/** @type {!HelpContentList} */
export const fakeHelpContentList = [
  {
    title: stringToMojoString16('Fix connection problems'),
    url: {url: 'https://support.google.com/chromebook/?q=6318213'},
    contentType: HelpContentType.kArticle
  },
  {
    title: stringToMojoString16(
        'Why won\'t my wireless mouse with a USB piece wor...?'),
    url: {url: 'https://support.google.com/chromebook/?q=123920509'},
    contentType: HelpContentType.kForum
  },
  {
    title: stringToMojoString16('Wifi Issues - only on Chromebooks'),
    url: {url: 'https://support.google.com/chromebook/?q=114174470'},
    contentType: HelpContentType.kForum
  },
  {
    title: stringToMojoString16('Network Connectivity Fault'),
    url: {url: 'https://support.google.com/chromebook/?q=131459420'},
    contentType: HelpContentType.kForum
  },
  {
    title: stringToMojoString16(
        'Connected to WiFi but can\'t connect to the internet'),
    url: {url: 'https://support.google.com/chromebook/?q=22864239'},
    contentType: HelpContentType.kUnknown
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

/** @type {!FeedbackContext} */
export const fakeFeedbackContext = {
  email: 'test.user2@test.com',
  pageUrl: {url: 'chrome://tab/'},
};

/** @type {!FeedbackContext} */
export const fakeEmptyFeedbackContext = {
  email: '',
  pageUrl: {url: ''},
};
