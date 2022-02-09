// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TopicSource} from '../personalization_app.mojom-webui.js';

/**
 * Stores ambient related states.
 */
export interface AmbientState {
  ambientModeEnabled: boolean;
  topicSource: TopicSource|null;
}

export function emptyState(): AmbientState {
  return {
    ambientModeEnabled: false,
    topicSource: null,
  };
}
