// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';

export interface CrossDeviceTab {
  title: string;
  url?: string;
  lastActiveElapsedText: string;
  device: string;
}

export class CrossDeviceTabsDelegate implements
    OrganizerListSectionDelegate<CrossDeviceTab> {
  init(_sectionClient: OrganizerListSectionClient) {}

  getHeader(): string {
    return loadTimeData.getString('tabsOnOtherDevices');
  }

  getItems(): Promise<Array<OrganizerListSectionItem<CrossDeviceTab>>> {
    const tabs: CrossDeviceTab[] = [
      {
        title: 'Google',
        url: 'https://www.google.com',
        lastActiveElapsedText: '5 mins ago',
        device: 'Pixel 8',
      },
      {
        title: 'YouTube',
        url: 'https://www.youtube.com',
        lastActiveElapsedText: '1 hour ago',
        device: 'Chromebook',
      },
      {
        title: 'GitHub',
        url: 'https://www.github.com',
        lastActiveElapsedText: 'Yesterday',
        device: 'MacBook Pro',
      },
    ];

    return Promise.resolve(
        tabs.map(tab => ({
                   title: [tab.title],
                   description: [
                     {text: tab.lastActiveElapsedText},
                     {text: tab.device},
                   ],
                   prefixIcon: tab.url ? {url: tab.url} : undefined,
                   data: tab,
                 })));
  }

  onItemClick(_item: OrganizerListSectionItem<CrossDeviceTab>) {}
}
