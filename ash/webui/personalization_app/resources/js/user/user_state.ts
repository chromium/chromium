// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {DefaultUserImage, UserImage, UserInfo} from '../../personalization_app.mojom-webui.js';

export interface UserState {
  defaultUserImages: DefaultUserImage[]|null;
  image: UserImage|null;
  info: UserInfo|null;
  profileImage: Url|null;
  isCameraPresent: boolean;
  lastExternalUserImage: UserImage|null;
  imageIsEnterpriseManaged: boolean|null;
}

export function emptyState(): UserState {
  return {
    defaultUserImages: null,
    image: null,
    info: null,
    profileImage: null,
    isCameraPresent: false,
    lastExternalUserImage: null,
    imageIsEnterpriseManaged: null,
  };
}
