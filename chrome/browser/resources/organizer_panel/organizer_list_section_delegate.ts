// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OrganizerListItem} from './organizer_list_item.js';

// Delegate for a section in the organizer list.
export interface OrganizerListSectionDelegate {
  // Returns the section header.
  getHeader(): string;

  // Returns all items that should be shown in the section.
  getItems(): OrganizerListItem[];
}
