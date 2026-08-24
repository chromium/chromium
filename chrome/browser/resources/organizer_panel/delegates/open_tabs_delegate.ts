// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';

export class OpenTabsDelegate implements OrganizerListSectionDelegate {
  getHeader(): string {
    // TODO(b/549784710): Use localized string.
    return 'Open Tabs';
  }

  getItems(): OrganizerListSectionItem[] {
    // TODO(b/549784710): Implement fetching open tabs.
    return [
      {
        title: 'Google',
        description: 'google.com',
        prefixIcon: {
          urls: ['https://www.google.com'],
        },
      },
      {
        title: 'YouTube',
        description: 'youtube.com',
        prefixIcon: {
          urls: ['https://www.youtube.com'],
        },
      },
      {
        title: 'Chromium',
        description: 'chromium.org',
        prefixIcon: {
          urls: ['https://www.chromium.org'],
        },
      },
    ];
  }
}
