// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Typedef to allow augmentation of Data objects sent from C++ to
 * JS for chrome://downloads.
 */

import {Data as MojomData} from './downloads.mojom-webui.js';

/** @typedef {MojomData | {hideDate: boolean}} */
export let Data;
