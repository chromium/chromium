// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {DefaultUserImage, UserInfo} from '../personalization_app.mojom-webui.js';

export interface UserState {
  defaultUserImages: Array<DefaultUserImage>|null;
  image: Url|null;
  info: UserInfo|null;
}

export function emptyState(): UserState {
  return {
    defaultUserImages: null,
    image: null,
    info: null,
  };
}
