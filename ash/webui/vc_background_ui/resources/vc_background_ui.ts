// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_router_element.js';

import {MantaStatusCode, SeaPenProviderInterface, SeaPenQuery} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {setSeaPenProviderForTesting} from 'chrome://resources/ash/common/sea_pen/sea_pen_interface_provider.js';
import {emptyState} from 'chrome://resources/ash/common/sea_pen/sea_pen_state.js';
import {getSeaPenStore} from 'chrome://resources/ash/common/sea_pen/sea_pen_store.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

// TODO(b/315031624) remove this class and connect to real implementation.
class FakeSeaPenProvider implements SeaPenProviderInterface {
  async searchWallpaper(_: SeaPenQuery) {
    return {images: [], statusCode: MantaStatusCode.kOk};
  }
  async selectSeaPenThumbnail(_: number) {
    return {success: false};
  }
  async selectRecentSeaPenImage(_: FilePath) {
    return {success: false};
  }
  async getRecentSeaPenImages() {
    return {images: []};
  }
  async getRecentSeaPenImageThumbnail(_: FilePath) {
    return {url: {url: ''}};
  }
  async deleteRecentSeaPenImage(_: FilePath) {
    return {success: false};
  }
}

setSeaPenProviderForTesting(new FakeSeaPenProvider());

getSeaPenStore().init(emptyState());
