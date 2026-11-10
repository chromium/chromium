// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory} from '../mojom/boca.mojom-webui.js';

export const {handler: pageHandler, callbackRouter} =
    browserProxyFactory.getInstance();
