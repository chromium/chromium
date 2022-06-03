// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
const template = document.createElement('template');
template.innerHTML = `
<dom-module id="edu-login-css">{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));
