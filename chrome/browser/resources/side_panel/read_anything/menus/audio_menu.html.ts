// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AudioMenuElement} from './audio_menu.js';

export function getHtml(this: AudioMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<grouped-action-menu
    id="menu"
    label="$i18n{audioTitle}"
    .menuGroups="${this.groups_}"
    .nonModal="${this.nonModal}"
    .closeOnClick="${false}"
    @highlight-change="${this.onHighlightChange_}"
    @open-accent-menu="${this.onOpenAccentMenu_}"
    @open-voice-selection-dialog="${this.onOpenVoiceSelectionDialog_}">
</grouped-action-menu>

${this.showVoiceSelectionDialog_ ? html`
  <voice-selection-dialog
      id="voiceSelectionDialog"
      .selectedVoice="${this.selectedVoice}"
      .availableVoices="${this.availableVoices}"
      .enabledLangs="${this.enabledLangs}"
      .localeToDisplayName="${this.localeToDisplayName}"
      .previewVoicePlaying="${this.previewVoicePlaying}"
      @close="${this.onVoiceSelectionDialogClose_}">
  </voice-selection-dialog>
` : ''}

${this.showAccentMenuDialog_ ? html`
  <accent-menu id="accentMenu"
      .enabledLangs="${this.enabledLangs}"
      .localeToDisplayName="${this.localeToDisplayName}"
      .selectedLang="${this.selectedLang}"
      .availableVoices="${this.availableVoices}"
      @close="${this.onAccentMenuClose_}">
  </accent-menu>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
