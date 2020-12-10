// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor} from '../module_descriptor.js';

/** @type {!ModuleDescriptor} */
export const kaleidoscopeDescriptor = new ModuleDescriptor(
    /*id=*/ 'kaleidoscope',
    /*heightPx=*/ 384,
    async () => null,
);
