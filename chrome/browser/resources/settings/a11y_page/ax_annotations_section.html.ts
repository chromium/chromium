// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsAxAnnotationsSectionElement} from './ax_annotations_section.js';

export function getHtml(this: SettingsAxAnnotationsSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<settings-toggle-button id="mainNodeAnnotationsToggle"
    class="hr"
    pref-key="settings.a11y.enable_main_node_annotations"
    label="$i18n{mainNodeAnnotationsTitle}"
    .subLabel="${this.getMainNodeAnnotationsToggleSublabel_()}">
</settings-toggle-button>
<!--_html_template_end_-->`;
  // clang-format on
}
