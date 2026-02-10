// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabStripInternalsView} from './tab_strip_internals_view.js';
import {TabStripInternalsViewModel} from './tab_strip_internals_viewmodel.js';

/**
 * Entry point for the TabStripInternals page.
 */
class TabStripInternalsApp {
  private viewmodel_: TabStripInternalsViewModel;

  constructor() {
    this.viewmodel_ = new TabStripInternalsViewModel();
    new TabStripInternalsView(this.viewmodel_);
    // Start the application.
    this.viewmodel_.initialize();
  }
}

// Bootstrap once DOM is ready.
document.addEventListener('DOMContentLoaded', () => new TabStripInternalsApp());
