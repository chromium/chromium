// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {pageHandler} from './page_handler.js';


// Function to make arbitrary visible UI change only when
// document.fullscreenElement is present - to help demonstrate the difference
// between the two fullscreen methods (probably better to do this through data
// binding).
function changeColor() {
  const header = document.querySelector('#header');
  if (document.fullscreenElement) {
    if (header.style.color === 'blue') {
      header.style.color = 'red';
    } else {
      header.style.color = 'blue';
    }
  }
}

document.addEventListener('DOMContentLoaded', function () {
  window.pageHandler = pageHandler;
  // Mojo API triggering native widget RequestFullscreen()
  const toggleFullscreenButton = document.querySelector('#toggle-fullscreen');
  toggleFullscreenButton.addEventListener('click',
      () => pageHandler.toggleFullscreen());

  // Web platform standard way of entering and exiting fullscreen
  const enterFullscreenButton = document.querySelector('#enter-fullscreen');
  enterFullscreenButton.addEventListener('click',
      () => document.documentElement.requestFullscreen());
  const exitFullscreenButton = document.querySelector('#exit-fullscreen');
  exitFullscreenButton.addEventListener('click',
      () => document.exitFullscreen());

  const changeColorButton = document.querySelector('#change-color');
  changeColorButton.addEventListener('click', () => changeColor());

  pageHandler.toggleFullscreen();
});
