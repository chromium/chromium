// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UserSkillsPageElement} from './user_skills_page.js';

// TODO(b/475607224): Instead of hardcoding, add resource strings for
// labels and names.
export function getHtml(this: UserSkillsPageElement) {
  // clang-format off
  return html`
<div id="header">
  <h1 id="title">Your skills</h1>
  <p id="subtitle">Skills help simplify and automate repetitive tasks</p>
</div>
<div id="empty-state">
  <picture>
    <source srcset="images/skills_empty_dark.svg"
        media="(prefers-color-scheme: dark)">
    <img src="images/skills_empty.svg" alt="No skills">
  </picture>
  <p id="headline">You'll find your skills here</p>
  <p id="notice-message">
    Skills help simplify and automate repetitive tasks
  </p>
</div>`;
  // clang-format on
}
