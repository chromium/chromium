// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import './icons.html.js';

import {AnchorAlignment, CrActionMenuElement, ShowAtPositionConfig} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert_ts.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './read_anything_toolbar.html.js';

export interface ReadAnythingToolbar {
  $: {
    menu: CrActionMenuElement,
  };
}


const ReadAnythingToolbarBase = WebUiListenerMixin(PolymerElement);
export class ReadAnythingToolbar extends ReadAnythingToolbarBase {
  static get is() {
    return 'read-anything-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private showAtPositionConfig_: ShowAtPositionConfig = {
    top: 20,
    left: 8,
    anchorAlignmentY: AnchorAlignment.AFTER_END,
  };


  private onShowSettingsMenuClick_() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('settings');
    assert(button);
    this.$.menu.showAt(button, this.showAtPositionConfig_);
  }
}

customElements.define('read-anything-toolbar', ReadAnythingToolbar);
