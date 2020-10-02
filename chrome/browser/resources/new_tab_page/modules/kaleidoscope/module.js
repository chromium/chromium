// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/origin.mojom-lite.js';

import {loadKaleidoscopeModule} from 'chrome://kaleidoscope/module.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {ModuleDescriptor} from '../module_descriptor.js';

/** @type {!ModuleDescriptor} */
export const kaleidoscopeDescriptor = new ModuleDescriptor(
    /*id=*/ 'kaleidoscope',
    /*heightPx=*/ 330,
    async () => {
      return loadKaleidoscopeModule();
    },
);
