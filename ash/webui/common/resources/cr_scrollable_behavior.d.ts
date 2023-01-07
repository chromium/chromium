// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

export {CrScrollableBehavior};

interface CrScrollableBehavior {
  updateScrollableContents(): void;
  requestUpdateScroll(): void;
  saveScroll(list: IronListElement): void;
  restoreScroll(list: IronListElement): void;
}

declare const CrScrollableBehavior: object;
