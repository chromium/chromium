// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './scanning_fonts_css.js';

const styleMod = document.createElement('dom-module');
styleMod.innerHTML = `{__html_template__}`;
styleMod.register('scanning-shared');
