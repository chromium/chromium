// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {SkillsWebview} from './skills_webview.js';
import {SKILLS_DIALOG_HOST_URL} from './skills_webview_bridge_constants.js';

const page = new SkillsWebview(SKILLS_DIALOG_HOST_URL);
page.init();
