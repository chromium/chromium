// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="chromeos_ash">
import '//resources/cr_elements/chromeos/cros_color_overrides.m.js';
// </if>
import '//resources/cr_elements/search_highlight_style.css.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import './settings_vars_css.js';

const styleMod = document.createElement('dom-module');
styleMod.innerHTML = `{__html_template__}`;
styleMod.register('settings-shared');
