// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file holds dependencies that are deliberately excluded
 * from the main app.js module, to force them to load after other app.js
 * dependencies. This is done to improve performance of initial rendering of
 * core elements of the landing page, by delaying loading of non-core
 * elements (either not visible by default or not as performance critical).
 */

import './customize_dialog.js';
import './middle_slot_promo.js';
import './voice_search_overlay.js';
import 'chrome://resources/cr_components/most_visited/most_visited.js';

export {CustomizeBackgroundsElement} from './customize_backgrounds.js';
export {CustomizeDialogElement} from './customize_dialog.js';
export {CustomizeModulesElement} from './customize_modules.js';
export {CustomizeShortcutsElement} from './customize_shortcuts.js';
export {MiddleSlotPromoElement} from './middle_slot_promo.js';
export {VoiceSearchOverlayElement} from './voice_search_overlay.js';
