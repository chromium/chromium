// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Entry file for extension
 */

import {Background} from './background.js';
import {DeskApiImpl} from './desk_api_impl.js';
import {NotificationApiImpl} from './notification_api_impl.js';

function main() {
  return new Background(chrome, new DeskApiImpl(), new NotificationApiImpl());
}

main();
