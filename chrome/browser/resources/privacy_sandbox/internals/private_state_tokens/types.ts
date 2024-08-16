// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export enum ItemsToRender {
  ISSUER_LIST = 'list',
  ISSUER_METADATA = 'metadata',
}

interface Metadata {
  issuerOrigin: string;
  expiration: string;
  purposes: string[];
}

interface ListItem {
  issuerOrigin: string;
  numTokens: number;
  redemptions: Redemption[];
  metadata: Metadata;
}

interface Redemption {
  origin: string;
  formattedTimestamp: string;
}

export const nullMetadataObj: Metadata = {
  issuerOrigin: '',
  expiration: '',
  purposes: [],
};

export type {ListItem, Metadata, Redemption};
