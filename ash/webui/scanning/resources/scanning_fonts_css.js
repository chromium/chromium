// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTemplate} from './scanning_fonts_css.html.js';

const styleMod = document.createElement('dom-module');
styleMod.appendChild(getTemplate().content);
styleMod.register('scanning-fonts');
