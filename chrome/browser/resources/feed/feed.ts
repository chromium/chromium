// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {feedSidePanelCallbackRouter, feedSidePanelHandler} from './feed_side_panel_handler.js';

feedSidePanelCallbackRouter.onEventOccurred.addListener(() => {
  document.querySelector<HTMLInputElement>('#mojo-event')!.value;
});
const mojoButton = document.querySelector('#do-something');
if (mojoButton) {
  mojoButton.addEventListener(
      'click', () => feedSidePanelHandler.doSomething());
}