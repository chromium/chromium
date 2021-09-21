// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GestureDetector} from './gesture_detector.js';

const plugin =
    /** @type {!HTMLEmbedElement} */ (document.querySelector('embed'));

const gestureDetector = new GestureDetector(plugin);
