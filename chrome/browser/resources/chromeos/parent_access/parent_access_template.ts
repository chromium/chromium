// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './parent_access_template.html.js';
import {ParentAccessResult, ParentAccessUiHandlerInterface} from './parent_access_ui.mojom-webui.js';
import {getParentAccessUiHandler} from './parent_access_ui_handler.js';

/**
 * The Parent Access template component wraps each Parent Access
 * screen with a title bar containing a close button to maintain
 * local control of title bar style in a way that avoids redundant
 * elements.
 */
class ParentAccessTemplate extends PolymerElement {
  static get is() {
    return 'parent-access-template';
  }

  static get template() {
    return getTemplate();
  }

  private parentAccessUiHandler: ParentAccessUiHandlerInterface;

  constructor() {
    super();
    this.parentAccessUiHandler = getParentAccessUiHandler();
  }

  private onParentAccessDialogClosed() {
    this.parentAccessUiHandler.onParentAccessDone(ParentAccessResult.kCanceled);
  }
}
customElements.define(ParentAccessTemplate.is, ParentAccessTemplate);
