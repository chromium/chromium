// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from './onc_mojo.js';

export namespace NetworkList {
  export enum CustomItemType {
    OOBE = 1,
    ESIM_PENDING_PROFILE = 2,
    ESIM_INSTALLING_PROFILE = 3,
  }

  export interface CustomItemData {
    iccid?: string;
  }

  export interface CustomItemState {
    customItemType?: NetworkList.CustomItemType;
    customItemName: string;
    customItemSubtitle?: string;
    polymerIcon: string|undefined;
    customData?: CustomItemData|string;
    showBeforeNetworksList?: boolean;
  }

  export type NetworkListItemType =
      OncMojo.NetworkStateProperties|CustomItemState;
}
