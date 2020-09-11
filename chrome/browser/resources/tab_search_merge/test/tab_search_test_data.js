// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function sampleData() {
  const profileTabs = {
    windows: [
      {
        active: true,
        tabs: [
          {
            index: 0,
            tabId: 1,
            favIconUrl: '',
            title: 'Google',
            url: 'https://www.google.com',
          },
          {
            index: 1,
            tabId: 5,
            favIconUrl: '',
            title: 'Amazon',
            url: 'https://www.amazon.com',
          },
          {
            index: 2,
            tabId: 6,
            favIconUrl: '',
            title: 'Apple',
            url: 'https://www.apple.com',
          }
        ],
      },
      {
        active: false,
        tabs: [
          {
            index: 0,
            tabId: 2,
            favIconUrl: '',
            title: 'Bing',
            url: 'https://www.bing.com/',
          },
          {
            index: 1,
            tabId: 3,
            favIconUrl: '',
            title: 'Yahoo',
            url: 'https://www.yahoo.com',
          },
          {
            index: 2,
            tabId: 4,
            favIconUrl: '',
            title: 'Apple',
            url: 'https://www.apple.com/',
          },
        ]
      }
    ]
  };

  return profileTabs;
}
