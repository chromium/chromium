// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const styleMod = document.createElement('dom-module');
styleMod.appendChild(html`{__html_template__}`.content);
styleMod.register('shimless-fonts');
