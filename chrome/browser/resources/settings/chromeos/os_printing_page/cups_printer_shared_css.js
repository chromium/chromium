// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/md_select_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '../../settings_shared_css.js';

const template = document.createElement('template');
template.innerHTML = `
<dom-module id="cups-printer-shared">{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));