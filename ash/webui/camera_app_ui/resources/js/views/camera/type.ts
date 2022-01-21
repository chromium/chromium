// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Mode} from '../../type.js';

import {CaptureHandler} from './mode/index.js';

export interface ModeConstraints {
  exact?: Mode;
  default?: Mode;
}

export type CameraViewUI = CaptureHandler;
