// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OrganizerListSectionItem} from './organizer_list_section_item.js';

// Interface implemented by the section element to receive update events.
export interface OrganizerListSectionClient {
  onItemsChanged(items: Array<OrganizerListSectionItem<unknown>>): void;
}

// Delegate for a section in the organizer list.
export interface OrganizerListSectionDelegate<T> {
  // Hooks up the section element to receive updates from this section's model.
  init(sectionClient: OrganizerListSectionClient): void;

  // Returns the section header.
  getHeader(): string;

  // Returns all items that should be shown in the section.
  getItems(): Promise<Array<OrganizerListSectionItem<T>>>;

  // Called when an item in this section is clicked.
  onItemClick(item: OrganizerListSectionItem<T>): void;
}
