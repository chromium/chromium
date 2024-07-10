// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface ListItem {
  issuerOrigin: string;
  numTokens: number;
  redemptions: Redemption[];
}

interface Redemption {
  origin: string;
  formattedTimestamp: string;
}

export const dummyListItemData: ListItem[] = [
  {
    issuerOrigin: 'issuer1.com',
    numTokens: 15,
    redemptions: [
      {origin: 'storeA.com', formattedTimestamp: '2024-06-18 14:30:00'},
      {origin: 'storeC.com', formattedTimestamp: '2024-06-20 11:00:00'},
    ],
  },
  {
    issuerOrigin: 'issuer2.com',
    numTokens: 7,
    redemptions:
        [{origin: 'storeB.com', formattedTimestamp: '2024-06-19 17:15:00'}],
  },
  {
    issuerOrigin: 'issuer3.com',
    numTokens: 0,
    redemptions: [],  // No redemptions for this issuer
  },
];

export type {ListItem, Redemption};
